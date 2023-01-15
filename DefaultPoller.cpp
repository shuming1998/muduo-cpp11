#include "EPollPoller.h"
#include "Poller.h"
#include <stdlib.h> // getenv() 获取环境变量

/*
 * newDefaultPoller 是 Poller 的一个静态方法，但不实现在 Poller 文件中
 * 因为 Poller 是一个基类，理论上不应该引用派生类的头文件
 * 通过该接口选择一个具体的I/O复用对象时，需要包含派生类对象的头文件
 * 所以从设计的合理性上来说，应该把这个函数实现在一个公共源文件中
 */

Poller *Poller::newDefaultPoller(EventLoop *loop) {
  if (::getenv("MUDUO_USE_POLL")) {
    // 生成 poll 的实例
    return nullptr;
  } else {
    // 生成 epoll 的实例
    return new EPollPoller(loop);
  }
}