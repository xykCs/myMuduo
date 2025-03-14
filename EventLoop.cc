#include "EventLoop.h"
#include "logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

// 防止一个线程创建多个EventLoop
thread_local EventLoop *t_loopInThisThread = nullptr;

// 默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

/*
eventfd机制:
    通过eventfd(initval, flags) 得到一个文件描述符 fd
    向fd写入一个uint64_t,会增加计数器initval,通知阻塞到读fd上的其他线程有事情处理
    从fd读取一个uint64_t,相当于消费通知,计数器会重置 (计数器为0,读取操作会阻塞)
*/

// wakeupfd => notify subReactor => newChannel
int createEventfd(){
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0){
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd()) 
    , wakeupChannel_(new Channel(this,wakeupFd_)) 
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread){
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else{
        t_loopInThisThread = this;
    }
    // 设置wakeupFd_发生读事件后的回调操作为:从wakeupFd_读(即接收mainloop的通知)
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 设置wakeupfd的事件类型为读 
    wakeupChannel_->enableReading();
    // 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件。当wakeupFd_有数据可读时，触发handleRead()消费通知
}

EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::handleRead(){
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one)); // 读取8字节
    if(n != sizeof(one)){
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

// 开始事件循环
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);
    
    while(!quit_){
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for(Channel *channel : activeChannels_){
            channel->handleEvent(pollReturnTime_);
        }
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for (const Functor &functor : functors)
    {
        functor(); 
    }
    callingPendingFunctors_ = false;
}

void EventLoop::quit(){
    quit_ = true;
    // 如果是在其他线程中调用的quit
    if(!isInLoopThread()){
        wakeup();   
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){ // 在当前loop线程中,执行cb
        cb();
    }
    else{ // 在其他线程中,执行当前loop的cb是不允许的. 需要唤醒loop所在线程执行cb
        queueInLoop(cb);
    }
}

// 把cb放入队列中，唤醒loop对应的线程，执行cb
void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // || callingPendingFunctors_: 当前loop正在自己线程中执行回调，但是loop又有了新的回调,
    //                             为了防止当前loop执行完这一轮后又阻塞在poll上,直接wakeup
    //                             让当前loop执行完后又被唤醒,继续执行新的回调
    if(!isInLoopThread() || callingPendingFunctors_){
        wakeup(); 
    }
}

// 唤醒loop对应的线程 : 向wakeupfd_写一个数据,wakeupChannel发生读事件,当前loop线程会被唤醒
// 当前loop在其他线程中执行wakeup(), 而当前loop对应的线程正阻塞
// 那么写入的wakeupFd_就是当前loop的wakeupFd_,然后wakeupChannel发生读事件
// 当前loop对应的线程就会由于监听的wakeupfd被唤醒(而非 由于client)
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

// EventLoop =》Poller
void EventLoop::updateChannel(Channel *channel){
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel){
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel){
    return poller_->hasChannel(channel);
}