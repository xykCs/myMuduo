# myMuduo
在学习并深入剖析muduo网络库源码的基础上，使用C++11对muduo网络库进行重构，去除了muduo对boost库的依赖，并根据muduo的设计思想，实现的基于多Reactor多线程模型的网络库。基于非阻塞IO和事件驱动的C++高并发TCP网络库，线程模型为one loop per thread。
