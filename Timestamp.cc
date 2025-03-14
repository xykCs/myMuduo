#include "Timestamp.h"

#include <time.h>

Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch){}

// 返回当前时间的Timestamp对象
Timestamp Timestamp::now(){
    // time(NULL)返回 时间戳（自Unix纪元到当前时间的秒数）
    return Timestamp(time(NULL)); // time_t -> int64_t 安全
}

std::string Timestamp::toString() const{
    char buf[128] = {0};
    // 将存储的时间戳 转为tm结构
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}
