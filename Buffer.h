#pragma once

#include "Logger.h"

#include <string>
#include <vector>

/*
 * 网络库底层的缓冲区类型定义
 * 应用写数据 => Buffer 缓冲区 => 内核中Tcp模块的发送缓冲区 => send
 * 主要用于防止用户写数据过快，直接将数据写到 Buffer 中
 *  +-------------------+----------------+-----------------+
 *  | prependable bytes | readable bytes | writable bytes  |
 *  |                   |    (CONTEND)   |                 |
 *  +-------------------+----------------+-----------------+
 *  |                   |                |                 |
 *  0        <=    readerIndex  <=  writerIndex    <=     size
 */

class Buffer {
public:
  // 记录缓冲区数据长度
  static const size_t kCheapPrepend = 8;
  static const size_t kInitialSize = 1024;

  explicit Buffer(size_t initialSize = kInitialSize)
      : buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend),
        writerIndex_(kCheapPrepend) {}

  // 可读长度
  size_t readableBytes() const { return writerIndex_ - readerIndex_; }
  // 可写长度
  size_t writableBytes() const { return buffer_.size() - writerIndex_; }
  // 可读起始指针
  size_t prependableBytes() const { return readerIndex_; }

  // 返回缓冲区中可读数据的起始地址
  const char *peek() const { return begin() + readerIndex_; }

  // onMessage Buffer -> string
  void retrieve(size_t len) {
    if (len < readableBytes()) {
      readerIndex_ += len; // 应用只读取了可读缓冲区数据的一部分， 还剩下 writerIndex_ - readerIndex_ += len
    } else { // len == readableBytes()
      retrieveAll();
    }
  }

  void retrieveAll() { readerIndex_ = writerIndex_ = kCheapPrepend; }

  // 把 onMessage 函数上报的 Buffer 数据转成string类型的数据
  // 读取 Buffer 中的所有可读数据
  std::string retrieveAllAsString() {
    return retrieveAsString(readableBytes());
  }

  // 读取 Buffer 中指定长度的数据
  std::string retrieveAsString(size_t len) {
    std::string result(peek(), len);
    // 数据已从缓冲区读出，复位缓冲区
    retrieve(len);
    return result;
  }

  void ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
      makeSpace(len);
    }
  }

  // 把 [data, data + len] 内存上的数据添加到 writable 缓冲区中，并移动
  // writerIndex_
  void append(const char *data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    writerIndex_ += len;
  }

  char *beginWrite() { return begin() + writerIndex_; }

  const char *beginWrite() const { return begin() + writerIndex_; }

  // 从 fd 上读取数据
  ssize_t readFd(int fd, int *saveErrno);
  // 通过 fd 发送数据
  ssize_t writeFd(int fd, int *saveErrno);

private:
  // 获取 vector 底层数组首元素地址，即数组起始地址
  char *begin() {
    // &(it.operator*())
    return &*buffer_.begin();
  }

  // 获取 vector 底层数组首元素地址常量，即数组起始地址
  const char *begin() const {
    // &(it.operator*())
    return &*buffer_.begin();
  }

  // 扩容 buffer_
  void makeSpace(size_t len) {
    /*
                  |读了一部分|↓
    kCheapPrepend |     reader    |     writer    |
    kCheapPrepend |              len              |

    读了一部分后，缓冲区已读出的部分就可以使用了(prependableBytes -
    kCheapPrepend) 如果已读数据长度 + kCheapPrepend + 可写长度 < 写入长度 +
    kCheapPrepend
   */
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
      // 将可写缓冲区长度扩充到 len
      buffer_.resize(writerIndex_ + len);
    } else { // 已读数据部分长度 + 可写长度足够使用
      // 将可读数据移动到前方，并将空出的空间分给可写缓冲区
      size_t readable = readableBytes();
      std::copy(begin() + readerIndex_, begin() + writerIndex_,
                begin() + kCheapPrepend);
      readerIndex_ = kCheapPrepend;
      writerIndex_ = readerIndex_ + readable;
    }
  }

  std::vector<char> buffer_;
  size_t readerIndex_;
  size_t writerIndex_;
};