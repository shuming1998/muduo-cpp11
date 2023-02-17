# muduo-Cpp11

**muduo 网络库作者：陈硕**

**项目地址**: https://github.com/chenshuo/muduo



**本项目旨在从开源网络库 muduo中寻求以下几方面知识：**

1. 理解 Linux 中的五种 I/O 模型，加深对 I/O 过程阻塞、非阻塞、同步、异步的认识，以及通过实践，更深刻地理解 epoll 网络编程的原理和优势。
2. 理解 Reactor 模型，以及 one loop per thread 的设计思想，掌握基于事件驱动、事件回调、 epoll和线程池的面向对象编程方法。
3. 学习整个项目优秀的代码设计和编写规范，以及学习 C++11 多种新特性如：mutex、smart pointer、function、bind等在项目中的实际应用。



**Reactor 模型**

主要组件：Event、Reactor、Demultiplex、EventHandler

![image-20230218012019296](https://github.com/shuming1998/muduo-cpp11/blob/main/image/reactor.png)



**muduo 中的 Multiple Reactors 模型**



![image-20230218012216496](https://github.com/shuming1998/muduo-cpp11/blob/main/image/multiple_reactors.png)





**重写的 muduo 网络库核心代码模块及功能：**

| 模块名称                  | 功能                                                         |
| :------------------------ | ------------------------------------------------------------ |
| Channel                   | 封装文件描述符 fd、该文件描述符上注册的事件 events、具体事件发生时返回的事件 revents、返回事件类型对应的回调函数；另外封装了一个 EventLoop 用于与 Poller 通信。 |
| Poller(EPollPoller)       | 对应于 Reactor 模型 中的 Demultiplex，封装了 epoll、该 epoll 中注册的 channels；另外封装了一个 EventLoop 与 Channel 通信。 |
| EventLoop                 | 对应于 Reactor 模型 中的 Reactor，是 Channel 和 Poller 之间通信的媒介，管理所有的 Channel 和一个 Poller；包含一个 wakeFd 和 wakeFdChannel，该 wakeFd 隶属于一个 subLoop， channel 事件发生时用于唤醒 subLoop 处理。 |
| Thread && EventLoopThread | Thread 封装了线程，EventLoopThread 封装了 Thread 和事件循环 EventLoop。 |
| EventLoopThreadPool       | 事件循环线程池，封装了一个用于监听网络连接事件的主事件循环、所有的EventLoopThread、以及它们对应的 EventLoop，如果不设置线程数，则只有一个主事件循环。如果设置了新线程，以 one loop per thread 的形式创建子线程和子事件循环；通过轮询的方式获取子事件循环。 |
| Socket                    | 封装 socket 通信相关操作                                     |
| Acceptor                  | 封装 Socket、 Channel、EventLoop，将 listenfd 打包为 acceptorChannel 交给主事件循环 baseLoop 处理。 |
| Buffer                    | 非阻塞 I/O 的缓冲区，应用层write -> Buffer -> Tcp send buffer -> send。 |
| TcpConnection             | 对应一个连接成功的客户端，封装了 Socket、Channel、读写消息的回调、消息发送完成后的回调、读\写缓冲区、控制数据写入速率的高水位线。 |
| TcpServer                 | 总领全局，封装了：所有的连接、运行在 mainLoop 中的 Acceptor、EventLoopThreadPool、有新连接时的回调、有读写消息的回调、消息发送完成的回调、EventLoop 线程初始化的回调。Acceptor 得到新连接并将其封装为一个 TcpConnection 对象，设置各类型的回调函数后，通过轮询的方式将其分发给子事件循环。 |



**编译和安装**

头文件拷贝到了：/usr/include/cmuduo

动态库文件拷贝到了：/usr/lib

```shell
$ git clone git@github.com:shuming1998/muduo-cpp11.git
$ cd muduo-cpp11
$ chmod +x autoBuild.sh
$ sudo ./autoBuild.sh
```



**回声测试用例**

```shell
$ cd muduo-cpp11/example
$ make clean
$ make -j32
$ ./test
# 另起终端
$ telnet 127.0.0.1 8000
```



**使用日志**

```c++
#include <cmuduo/Logger.h>
int a = 10;
std::string log = "hello world!";

LOG_INFO("cmuduo log: %d, %s", a, log);
LOG_ERROR("cmuduo log: %d, %s", a, log);
LOG_FATAL("cmuduo log: %d, %s", a, log);
LOG_DEBUG("cmuduo log: %d, %s", a, log);
```

