#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/*
 * 从 fd 上读取数据，Poller 工作在 LT 模式
 * Buffer 缓冲区是有大小的，但从 fd 上读数据时，却不知道 tcp 数据最终的大小
 * 如果 Buffer 过大，内存浪费；如果 Buffer 太小，数据读不完
 * 借助 readv 系统调用，在使用 Buffer 的同时，在栈上开辟一块足够大的 buffer
 */
ssize_t Buffer::readFd(int fd, int *saveErrno) {
  char extrabuf[65536] = {0}; // 栈上的内存空间 64k，开辟速度快，出作用域操作系统自动回收内存
  // 每个 iovec 结构体对象有两个成员属性：缓冲区地址；缓冲区长度
  // vec 是一个可以表示多个缓冲区的数组，供 readv 使用,将数据填充到这些缓冲区中
  struct iovec vec[2];
  const size_t writable = writableBytes(); // Buffer 缓冲区剩余的可写空间大小，不一定足够存储 fd 发来的数据
  // 第一块缓冲区
  vec[0].iov_base = begin() + writerIndex_;
  vec[0].iov_len = writable;
  // 第二块缓冲区
  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof extrabuf;

  // 如果 buffer 空间足够，就不往 extrabuf 中读数据
  // 如果使用了 extrabuf，限制最多读128k - 1个字节
  const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
  // readv 系统调用可以将从 fd 上读到的数据写入到多块非连续缓冲区中
  const ssize_t n = ::readv(fd, vec, iovcnt);
  if (n < 0) {
    *saveErrno = errno;
  } else if (n <= writable) { // Buffer 的可写缓冲区 已经够存储从 fd 中读出的数据
    // 数据已经通过 readv 读进去了
    writerIndex_ += n;
  } else {
    // 原缓冲区写满了
    writerIndex_ = buffer_.size();
    // 将剩余的数据写到 extrabuf 中，从 writerIndex_ 开始写 n - writable
    // 大小的数据
    append(extrabuf, n - writable);
  }
  return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno) {
  ssize_t n = ::write(fd, peek(), readableBytes());
  if (n < 0) {
    *saveErrno = errno;
  }
  return n;
}