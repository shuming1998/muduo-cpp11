#pragma once

#include "Timestamp.h"
#include "noncopyable.h"
#include <functional>
#include <memory>

/*
 * 一个 EventLoop 包含一个 Poller， 监听多个 sockfd，sockfd 封装于
 * channel，它们在 Reactor 模型上对应多路事件分发器 Channel 理解为通道，封装了
 * sockfd 和其感兴趣的 event，如EPOLLIN、EPOLLOUT 还绑定了 poller 返回的具体事件
 *
 * channel 无法直接与 poller 通信，需要借助 EventLoop 中的 update/removeChannel
 * 间接调用 poller 中的 update/removeChannel, 从而完成 poller 中 epoll_ctl 与
 * channel 的通信
 *
 * channel 一共有两种，一种是 acceptor 中封装 listenfd 的channel，另一种是封装用户连接 connfd 的channel
 * connfd 的回调函数都是由 TcpConnection 设置的
 */

class EventLoop;

class Channel : noncopyable {
public:
  // typedef std::function<void()> EventCallback;
  using EventCallback = std::function<void()>;
  // typedef std::function<void(TimeStamp)> ReadEventCallback;
  using ReadEventCallback = std::function<void(Timestamp)>;

  Channel(EventLoop *loop, int fd);
  ~Channel();
  // fd 得到 poller 通知以后，处理事件,调用相应的回调
  void handleEvent(Timestamp receiveTime);

  // 设置回调函数对象
  // 赋值的时候把左值更改为右值，能减少一次临时对象的生成和赋值
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }
  // 在 Acceptor 对象的构造函数中调用，绑定的是 Acceptor::handleRead，在
  // Acceptor::handleRead 中会执行用户通过 TcpServer 绑定的自定义的回调函数
  void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }

  // 防止当 channel 被手动 remove 掉之后(通过EventLoop)，channel
  // 还在执行上面四个回调操作
  void tie(const std::shared_ptr<void> &);

  // 返回 fd
  int fd() const { return fd_; }
  // 返回 fd 感兴趣的事件
  int events() const { return events_; }

  // 设置感兴趣事件中真正发生的事件
  // channel 无法监听fd发生的事件，是 poller
  // 在监听，监听到事件后，通过这个公有接口设置 channel
  void set_revents(int revt) { revents_ = revt; }

  // 设置 fd 相应的事件状态
  // 对读事件感兴趣，并通过update()接口调用 poller 中的
  // epoll_ctl，将感兴趣的读事件添加到epoll中
  void enableReading() {
    events_ |= kReadEvent;
    update();
  }
  void disableReading() {
    events_ &= ~kReadEvent;
    update();
  }
  void enableWriting() {
    events_ |= kWriteEvent;
    update();
  }
  void disableWriting() {
    events_ &= ~kWriteEvent;
    update();
  }
  void disableAll() {
    events_ = kNoneEvent;
    update();
  }

  // 返回 fd 当前的事件状态
  // 确定是否有事件
  bool isNoneEvent() const { return events_ == kNoneEvent; }
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }

  int index() { return index_; }
  void set_index(int idx) { index_ = idx; }

  // on loop per thread
  EventLoop *ownerLoop() { return loop_; }
  void remove();

private:
  void update();
  // 处理受保护的事件
  void handleEventWithGuard(Timestamp receiveTime);

  // 描述三种fd的状态
  static const int kNoneEvent;  // 没有任何感兴趣事件
  static const int kReadEvent;  // 读事件
  static const int kWriteEvent; // 写事件

  EventLoop *loop_; // channel 所属的事件循环，用于和 poller 通信
  const int fd_;    // 要往 poller 上注册的文件描述符，fd Poller 监听的对象
  int events_;      // 注册 fd 感兴趣的事件(读事件、写事件等)
  int revents_;     // poller 通知的具体发生的事件
  int index_;       // used by Poller

  /*
   * 借助 shared_ptr 和 weak_ptr
   * 解决了多线程访问共享对象的线程安全问题：线程安全的对象回调与析构 线程 A
   * 和线程 B 访问一个共享的对象，如果线程A正在析构这个对象的时候 线程 B
   * 又要调用该共享对象的成员方法，此时可能线程A已经把对象析构完了，线程B再去访问该对象，就会发生不可预期的错误
   * weak_ptr
   * 不会改变资源的引用计数，是一个观察者，通过观察 shared_ptr
   * 来判定资源是否存在 weak_ptr
   * 持有的引用计数，不是资源的引用计数，而是同一个资源的观察者的计数
   * weak_ptr没有提供常用的指针操作，无法直接访问资源，需要先通过 lock
   * 方法提升为 shared_ptr 强智能指针，才能访问资源
   */
  std::weak_ptr<void> tie_;
  bool tied_;

  // 因为 channel 能够获知 fd 最终发生的具体的事件 revents
  // 所以它负责根据 revents 具体发生事件的类型，来选择调用具体事件的回调操作
  // 这些回调函数是由用户实现的，只不过通过 TcpConnection 接口间接传给
  // channel，由 channel 在相应事件 revents_ 发生时调用
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};