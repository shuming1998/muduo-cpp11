#include "TcpConnection.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

#include <errno.h>
#include <functional>
#include <memory>
#include <string>

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
  if (loop == nullptr) {
    LOG_FATAL("[%s:%s:%d]\nTcpConnectionLoop is null!\n", __FILE__,
              __FUNCTION__, __LINE__);
  }
  return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg,
                             int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64 * 1024 * 1024) { // 64M
  // 给 channel 设置相应的回调函数，poller 给 channel 通知感兴趣的事件发生时，channel 会回调相应的操作函数
  // 新连接 handleRead 中调用的 messageCallback_ 就是用户在构造函数中通过 setMessageCallback 设置的 onMessage
  channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
  channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
  channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
  channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
  LOG_INFO("TcpConnection::ctor[%s] at fd = %d\n", name_.c_str(), sockfd);
  socket_->setKeepAlive(true); // TCP 心跳包，保活机制
}

TcpConnection::~TcpConnection() {
  LOG_INFO("TcpConnection::dtor[%s] at fd = %d state = %d\n", name_.c_str(),
           channel_->fd(), (int)state_);
}

void TcpConnection::handleRead(Timestamp receiveTime) {
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0) {
    // 已建立连接的用户有可读事件发生了，调用用户传入的回调操作 onMessage
    // shared_from_this() 表示传递的是智能指针
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  } else if (n == 0) {
    handleClose();
  } else {
    errno = savedErrno;
    LOG_ERROR("TcpConnection::handleRead");
    handleError();
  }
}

void TcpConnection::handleWrite() {
  if (channel_->isWriting()) {
    int savedErrno = 0;
    ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
    if (n > 0) {
      outputBuffer_.retrieve(n);
      // 可读数据为 0 说明数据已经全部发送出去了
      if (0 == outputBuffer_.readableBytes()) {
        channel_->disableWriting();
        if (writeCompleteCallback_) {
          // 唤醒该 loop
          // 对应的线程，执行回调（其实直接调用loop_->writeCompleteCallback_(shared_from_this()就可以)）
          loop_->queueInLoop(
              std::bind(writeCompleteCallback_, shared_from_this()));
        }
        // 用户调用了
        // TcpConnection::shutdown，但此时发送缓冲区还在发送数据，还没真正
        // shutdown
        if (state_ == kDisconnecting) {
          shutdownInLoop();
        }
      }
    } else if (n <= 0) {
      LOG_ERROR("[%s:%s:%d]\nTcpConnection::handleWrite\n", __FILE__,
                __FUNCTION__, __LINE__);
    }
  } else {
    LOG_ERROR("[%s:%s:%d]\nTcpConnection fd = %d is down, no more writing\n",
              __FILE__, __FUNCTION__, __LINE__, channel_->fd());
  }
}

// Poller 通知 channel 调用 Channel::closeCallback 方法 =>
// TcpConnection::handleClose =>
void TcpConnection::handleClose() {
  LOG_INFO("[%s:%s:%d]\nfd = %d state = %d\n", __FILE__, __FUNCTION__, __LINE__,
           channel_->fd(), (int)state_);
  setState(kDisconnected);
  channel_->disableAll();
  // 获取当前对象的智能指针
  TcpConnectionPtr connPtr(shared_from_this());
  // 执行用户注册的连接关闭的回调函数
  if (connectionCallback_) {
    connectionCallback_(connPtr);
  } else if (!connectionCallback_) {
    LOG_ERROR("[%s:%s:%d]\nconnectionCallback_ did't set!\n", __FILE__,
              __FUNCTION__, __LINE__);
  }
  // 执行 TcpServer::removeConnection 回调函数
  if (closeCallback_) {
    closeCallback_(connPtr);
  } else if (!closeCallback_) {
    LOG_ERROR("[%s:%s:%d]\ncloseCallback_ did't set!\n", __FILE__, __FUNCTION__,
              __LINE__);
  }
}

void TcpConnection::handleError() {
  int err = Socket::getSocketError(channel_->fd());
  LOG_ERROR("[%s:%s:%d]\nTcpConnection::handleError name: %s - SO_ERROR: %d\n",
            __FILE__, __FUNCTION__, __LINE__, name_.c_str(), err);
}

void TcpConnection::send(const std::string &buf) {
  if (state_ == kConnected) {
    if (loop_->isInLoopThread()) {
      sendInLoop(buf.c_str(), buf.size());
    } else {
      loop_->runInLoop(
          std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
    }
  }
}

// 发送数据
// 应用写数据速度(快)与内核发送数据速度(慢)不匹配，应将待发送的数据写入缓冲区，并设置水位回调
void TcpConnection::sendInLoop(const void *data, size_t len) {
  ssize_t nwrote = 0;     // 本次发送数据的长度
  size_t remaining = len; // 剩余数据的长度
  bool faultError = false;
  // 之前调用过该 connect 的 shutdown，不能再进行发送了
  if (state_ == kDisconnected) {
    LOG_ERROR("[%s:%s:%d]\ndisconnected, give up writing!\n", __FILE__,
              __FUNCTION__, __LINE__);
    return;
  }
  // 最初设置的新连接的 channel_ 只对读事件感兴趣
  // 条件列表表示该 channel_ 第一次开始写数据，而且缓冲区没有待发送数据
  if (!channel_->isWriting() && 0 == outputBuffer_.readableBytes()) {
    nwrote = ::write(channel_->fd(), data, len);
    // 数据发送成功
    if (nwrote >= 0) {
      // 判断数据是否发送完
      remaining = len - nwrote;
      // 数据一次性全部发送完成了，所以无需给 channel 设置 epollout 事件
      if (0 == remaining && writeCompleteCallback_) {
        loop_->queueInLoop(
            std::bind(writeCompleteCallback_, shared_from_this()));
      }
      // 数据发送出错
    } else {
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG_ERROR("[%s:%s:%d]\nTcpConnection::sendInLoop\n", __FILE__,
                  __FUNCTION__, __LINE__);
        // 对方关闭了连接/自己关闭了发送连接 或者对端重启连接后还未连接
        if (errno == EPIPE || errno == ECONNRESET) {
          faultError = true;
        }
      }
    }
  }
  // 当前这次 write 并未将全部数据发送出去，需要将剩余数据保存到缓冲区中
  // 然后给 channel_ 注册 EPOLLOUT 事件，poller 发现 tcp 的发送缓冲区有空间时
  // 会通知 sockfd(channel)，调用 channel_ 的 writeCallback_ 回调方法
  // (TcpConnection::handleWrite->channel_)
  // 直到把发送缓冲区的数据全部发送完成为止
  if (!faultError && remaining > 0) {
    size_t oldLen = outputBuffer_.readableBytes(); // 缓冲区剩余待发送数据长度
    if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ &&
        highWaterMarkCallback_) {
      loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(),
                                   oldLen + remaining));
    }
    outputBuffer_.append((char *)data + nwrote, remaining);
    if (!channel_->isWriting()) {
      // 这里一定要注册 channel 的写事件，否则即使有剩余数据，poller 也不会给channel_ 通知
      //  EPOLLOUT，继而无法驱动 channel_ 调用 writeCallback，即 TcpConnection::handleWrite
      channel_->enableWriting();
    }
  }
}

// 连接建立，创建连接时调用
void TcpConnection::connectEstablished() {
  setState(kConnected);
  // TcpConnection 会给到用户手里，channel_ 中的回调调用的是 TcpConnection
  // 的成员方法 初始化弱智能指针，后续用于防止 TcpConnection
  // 对象已经析构，而 channel 对象又调用了它的成员方法而产生未定义行为的情况
  channel_->tie(shared_from_this());
  channel_->enableReading(); // 向对应的 poller 注册 channel 的 EPOLLIN 读事件
  // 新连接建立，执行回调
  connectionCallback_(shared_from_this());
}

// 连接销毁，连接关闭时调用
void TcpConnection::connectDestroyed() {
  if (state_ == kConnected) {
    setState(kDisconnected);
    channel_->disableAll(); // 把 channel_ 所有感兴趣的事件 delete
    connectionCallback_(shared_from_this());
  }
  channel_->remove(); // 把 channel 从 poller 中删除调
}

// 关闭连接
void TcpConnection::shutdown() {
  if (state_ == kConnected) {
    setState(kDisconnecting);
    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
  }
}

void TcpConnection::shutdownInLoop() {
  // channel_ 已经将发送缓冲区 outputBuffer 中的数据发送完了
  if (!channel_->isWriting()) {
    // 关闭 sockfd 的 write 端，poller 给 channel 通知 EPOLLHUB 事件，
    // 触发 channel::handleEventWithGuard 中的 closeCallback_ 回调函数
    // closeCallback_ 即 TcpConnection 在构造函数中注册的
    // TcpConnection::handleClose 方法
    socket_->shutdownWrite();
  }
}