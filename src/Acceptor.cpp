#include "Acceptor.h"
#include "InetAddress.h"
#include "Logger.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// 创建非阻塞 socket
static int createNonblocking() {
  int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (sockfd < 0) {
    LOG_FATAL("[%s:%s:%d]\nlisten socket create error: %d!\n", __FILE__,
              __FUNCTION__, __LINE__, errno);
  }
  return sockfd;
}

// 有新用户连接时，最终相应的就是 TcpServer::newConnection 方法
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop) // 通过 loop 获取 poller 从而将新连接打包好的 channel 发送给 poller
    , acceptSocket_(createNonblocking()) // 1. 创建非阻塞的 listenFd
    , acceptChannel_(loop, acceptSocket_.fd()) // 封装 acceptChannel_，通过 mainLoop 完成在 poller 上的监听
    , listenning_(false) {
  acceptSocket_.setReuseAddr(true);      // 2. 设置 sockOption
  acceptSocket_.setReusePort(true);
  acceptSocket_.bindAddress(listenAddr); // 3. bind 刚才创建的 socket
  //! Acceptor 只设置 readCallback，因为它只关心新用户连接事件，而在
  //! TcpConnection 中则关心已连接用户的所有事件
  // TcpServer::start() 会调用 Acceptor.listen()
  // 每当有新用户连接时，要执行一个回调函数 这个回调函数将 connfd 打包成 channel 并轮询唤醒一个 subloop，将 channel 交给它
  // 所以在还未发生事件时，先给打包好的 channel 注册回调函数，然后在 handleRead 中，会执行针对新连接的回调函数
  // newConnectionCallback_：通过 TcpServer 的回调函数设置的
  acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this)); // 4. 设置 readCallback
}

Acceptor::~Acceptor() {
  acceptChannel_.disableAll();
  acceptChannel_.remove();
}

void Acceptor::listen() {
  listenning_ = true;
  acceptSocket_.listen();         // listen
  acceptChannel_.enableReading(); // 将 acceptChannel_ 注册到 poller 中
}

// 当 listenfd 有新用户连接时调用
void Acceptor::handleRead() {
  InetAddress peerAddr;
  // 通过传引用的方式，获取客户端地址信息
  int connfd = acceptSocket_.accept(&peerAddr); // 返回服务器端与客户端通信时的 fd
  if (connfd >= 0) {
    // 这里的 newConnectionCallback_ 是在 TcpServer 的构造函数中注册的 TcpServer::newConnection 方法
    if (newConnectionCallback_) {
      // 轮询找到 subLoop 唤醒，分发当前的新客户端的 channel
      newConnectionCallback_(connfd, peerAddr); // 执行 TcpServer 在构造函数中绑定的newConnection回调函数
    } else {
      ::close(connfd);
    }
  } else {
    LOG_ERROR("[%s:%s:%d]\naccept error:%d!\n", __FILE__, __FUNCTION__,
              __LINE__, errno);
    // 此进程可用的文件描述符资源达到上限，实际处理时可以调整进程 fd
    // 上限，或者设置服务器集群，实现分布式部署
    if (errno == EMFILE) {
      LOG_ERROR("[%s:%s:%d]\nsockfd reached limit!", __FILE__, __FUNCTION__,
                __LINE__);
    }
  }
}
