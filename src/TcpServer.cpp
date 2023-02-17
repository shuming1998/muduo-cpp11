#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
  if (loop == nullptr) {
    LOG_FATAL("[%s:%s:%d]\nmainLoop is null!\n", __FILE__, __FUNCTION__,
              __LINE__);
  }
  return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr,
                     const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop)) // baseLoop
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)) // 处理新用户连接
    , threadPool_(new EventLoopThreadPool(loop, name_))               // 事件循环线程池，这里只是创建，未开启事件循环
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0) { // 原子整形 started_ 用来保证 server 只启动一次
  // 1. 在 TcpServer 的构造函数中，将 acceptor_ 的 newConnectionCallback_ 绑定为 TcpServer::newConnection
  // 2. 在 Acceptor 的构造函数中，将 acceptorChannel 的 readCallback_ 绑定为 Acceptor::handleRead
  // 4. 在 Acceptor::handleRead 中，会调用 newConnectionCallback_，即 TcpServer::newConnection
  // 3. 当有新用户连接时，acceptorChannel 执行 readCallback_,即 Acceptor::handleRead
  acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                std::placeholders::_1,
                                                std::placeholders::_2));
}

// 设置底层 loop 个数
void TcpServer::setThreadNum(int numThreads) {
  threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听(开启 Acceptor 的 listen)
void TcpServer::start() {
  // 防止一个 TcpServer 对象被 start 多次
  if (started_++ == 0) {
    threadPool_->start(threadInitCallback_); // 启动底层的 loop 线程池，创建子线程(如果设置了的话)
    // 把 acceptor 中的 acceptChannel_ 注册在 mainLoop 的 poller 上，监听新用户连接
    loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
  }
}

// 有一个新客户端连接时，会通过 acceptorChannel 执行这个回调函数
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
  // 轮询算法选择一个 subLoop 来管理 channel
  EventLoop *ioLoop = threadPool_->getNextLoop();
  char buf[64] = {0};
  // 设置新连接名称
  snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  std::string connName = name_ + buf;

  LOG_INFO("[%s:%s:%d]\nTcpServer::newConnection [%s] - new connection [%s] "
           "from %s\n",
           __FILE__, __FUNCTION__, __LINE__, name_.c_str(), connName.c_str(),
           peerAddr.toIpPort().c_str());

  // 通过 sockfd 获取其绑定的本机的IP地址和端口号信息
  sockaddr_in local;
  ::bzero(&local, sizeof(local));
  socklen_t addrLen = sizeof(local);
  if (::getsockname(sockfd, (sockaddr *)&local, &addrLen) < 0) {
    LOG_ERROR("[%s:%s:%d]\nsockets::getLocalAddr\n", __FILE__, __FUNCTION__,
              __LINE__);
  }
  InetAddress localAddr(local);

  // 根据连接成功的 sockfd，创建一个 TcpConnection 连接对象
  TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
  connections_[connName] = conn;
  // 设置相应的回调
  // 下面的回调都由用户设置给 TcpServer => TcpConnection => Channel=> Poller => notify channel 调用回调
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  // 设置如何关闭连接的回调
  // 用户会调用 conn->shutdown() => shutdownInLoop => Socket::shutdownWrite
  // => poller 给 channel 上报 EPOLLHUB => Channel::handleWithGuard 调用 closeCallback_
  // => TcpConnection::handleClose => TcpServer::removeConnection
  conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
  // 直接调用 TcpConnection::connectEstablished，建立连接
  // 这里的 ioLoop 也有可能是主线程 mainLoop(用户没有设置threadNum)
  // 如果 ioLoop 执行的回调不是在当前 Loop，runInLoop 中就会执行 queueInLoop，唤醒对应的 loop 执行回调 connectEstablished
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
  loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
  LOG_INFO("[%s:%s:%d]\nTcpServer::removeConnectionInLoop [%s] - connection %s\n",
      __FILE__, __FUNCTION__, __LINE__, name_.c_str(), conn->name().c_str());
  connections_.erase(conn->name());
  EventLoop *ioLoop = conn->getLoop();
  // 将 channel 从 poller 中删除
  ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}

TcpServer::~TcpServer() {
  LOG_INFO("[%s:%s:%d]\nTcpServer::~TcpServer [%s] destructing!\n", __FILE__,
           __FUNCTION__, __LINE__, name_.c_str());
  for (auto &item : connections_) {
    // 获取一个 TcpConnection 的局部智能指针对象，出作用域自动释放 new 出来的
    // TcpConnection 对象资源
    TcpConnectionPtr conn(item.second);
    // 释放后就无法再访问原来指向的 TcpConnection 对象了，所以才定义了上面的
    // conn
    item.second.reset();
    // 然后通过 conn 调用 TcpConnection::connectDestroyed，销毁连接
    conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
  }
}