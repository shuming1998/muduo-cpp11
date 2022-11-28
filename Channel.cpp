#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

// EPOLLIN 连接到达或有数据来临；EPOLLPRI 外带数据； EPOLLOUT 有数据要写
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop: ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {}

Channel::~Channel() {}

// 一个 TcpConnection 新连接创建的时候被调用
void Channel::tie(const std::shared_ptr<void> &obj) {
  tie_ = obj;
  tied_ = true;
}

// 当改变 channel 所表示的 fd 的事件后，需要通过 update() 函数注册 poller 中 fd
// 相应的事件
void Channel::update() {
  // channel 和 poller 互不所属，但都属于一个 EventLoop
  // 通过channel 所属的 EventLoop 调用 poller 的相应方法， 注册 fd 的events事件
  // 将channel 作为参数传递给 loop_中的 updateChannel 方法
  loop_->updateChannel(this);
}

// 在channel 所属的 EventLoop 的 ChannelList 中，把当前的 channel 删除掉
void Channel::remove() { loop_->removeChannel(this); }

void Channel::handleEvent(Timestamp receiveTime) {
  // 弱回调：如果对象还活着，就调用它的成员函数，否则忽略之
  if (tied_) {
    std::shared_ptr<void> guard = tie_.lock();
    // 将弱智能指针提升为强智能指针，以便访问资源
    if (guard) {
      handleEventWithGuard(receiveTime);
    }
    // 提升失败，原对象已经被析构
  } else {
    handleEventWithGuard(receiveTime);
  }
}

// 根据 poller 通知的 channel 发生的具体事件，由 channel 负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime) {
  LOG_INFO("[%s:%s:%d]\nchannel handleEvent revents: %d\n", __FILE__,
           __FUNCTION__, __LINE__, revents_);
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    if (closeCallback_) {
      closeCallback_();
    }
  }

  if (revents_ & EPOLLERR) {
    if (errorCallback_) {
      errorCallback_();
    }
  }

  if (revents_ & (EPOLLIN | EPOLLPRI)) {
    if (readCallback_) {
      readCallback_(receiveTime);
    }
  }

  if (revents_ & EPOLLOUT) {
    if (writeCallback_) {
      writeCallback_();
    }
  }
}