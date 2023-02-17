#pragma once



#include <memory>
#include <functional>

class Buffer;
class Timestamp;
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
// 后续使用这些函数对象的时候，第一个参数传递的是智能指针(by shared_from_this)
// TcpConnection 会把方法传递给 channel，等channel 调用这些方法的时候，可能 TcpConnection 对象已经被析构了
// 所以在 channel 中有一个 weak_ptr
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer *, Timestamp)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;
