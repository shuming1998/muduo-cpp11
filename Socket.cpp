#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"

#include <netinet/tcp.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Socket::~Socket() { close(sockfd_); }

void Socket::bindAddress(const InetAddress &localaddr) {
  if (0 != ::bind(sockfd_, (sockaddr *)localaddr.getSockAddr(),
                  sizeof(sockaddr_in))) {
    LOG_FATAL("[%s:%s:%d]\nbind sockfd: %d fail!\n", __FILE__, __FUNCTION__,
              __LINE__, sockfd_);
  }
}

void Socket::listen() {
  if (0 != ::listen(sockfd_, 1024)) {
    LOG_FATAL("[%s:%s:%d]\nlisten sockfd: %d fail!\n", __FILE__, __FUNCTION__,
              __LINE__, sockfd_);
  }
}

// 通过返回值返回通信时用的 fd，并通过输出参数返回客户端通信地址和端口号
int Socket::accept(InetAddress *peeraddr) {
  sockaddr_in addr;
  socklen_t len = sizeof addr;
  bzero(&addr, sizeof addr);
  // 设置 connfd 为非阻塞，并且关闭父进程的文件描述符
  int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (connfd >= 0) {
    peeraddr->setSockAddr(addr);
  }
  return connfd;
}

void Socket::shutdownWrite() {
  if (::shutdown(sockfd_, SHUT_WR) < 0) {
    LOG_ERROR("[%s:%s:%d]\nshutdown write error!\n", __FILE__, __FUNCTION__,
              __LINE__);
  }
}
void Socket::setTcpNoDelay(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

void Socket::setReuseAddr(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::setReusePort(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}

int Socket::getSocketError(int sockfd) {
  int optval;
  socklen_t optlen = static_cast<socklen_t>(optval);
  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return errno;
  } else {
    return optval;
  }
}