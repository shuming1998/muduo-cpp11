![2022-11-23 13-46-25 的屏幕截图](/home/cui/图片/2022-11-23 13-46-25 的屏幕截图.png)

开启多线程后

​		mainReactor负责监听新的客户端连接，并将连接的客户端的sockfd打包成channel，通过负载均衡算法(轮询)将其分发给工作线程。

​		工作线程上的subReactor代表一个EventLoop，每个EventLoop都监听一组channel，这组channel中的每个channel都在自己的工作线程中执行(通过系统调用获取的tid来判断EventLoop属于哪个工作线程)

​		EventLoop调用多路事件分发器，事件分发器epoll_wait监听发生的事件，将发生的activateChannels返回给EventLoop