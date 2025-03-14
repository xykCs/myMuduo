#include "Channel.h"
#include "EventLoop.h"
#include "logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop) 
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{
}

Channel::~Channel(){}

// 新建TcpConnection时调用
void Channel::tie(const std::shared_ptr<void> &obj){
    tie_ = obj;
    tied_ = true;
}

// 当改变Channel所封装fd的events事件后，update负责在poller里更改fd相应事件(epoll_ctl)
// EventLoop => 1个Poller + 若干Channel
void Channel::update(){
    loop_->updateChannel(this);
}

void Channel::remove(){
    loop_->removeChannel(this);
}

// fd得到poller通知以后, 调用相应的回调方法
void Channel::handleEvent(Timestamp receiveTime){
    if(tied_){
        std::shared_ptr<void> guard = tie_.lock();
        if(guard){
            handleEventWithGuard(receiveTime);
        }
    }
    else{
        handleEventWithGuard(receiveTime);
    }
}
// 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime){
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if(closeCallback_){
            closeCallback_();
        }
    }
    if(revents_ & EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }
    if(revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }
    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}