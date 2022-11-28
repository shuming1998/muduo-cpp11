# My_Muduo_With_Cpp-11

#### 1、编译：

```
$ chmod +x autoBuild.sh
$ sudo ./autoBuild.sh
```

头文件拷贝到了：/usr/include/mymuduo

动态库文件拷贝到了：/usr/lib

#### 2、使用：

在头文件中 #include <mymuduo/TcpServer.h>

使用日志：#include <mymuduo/Logger.h>	(方法类似pringtf)

```c++
LOG_INFO("myMuduo: %d", int);

LOG_ERROR("myMuduo: %d", int);

LOG_FATAL("myMuduo: %d", int);

LOG_DEBUG("myMuduo: %d", int);
```

#### 3、回声测试实例：

```shell
$ cd example
$ make clean
$ make
$ ./test
# 另起终端
$ telnet 127.0.0.1 8000
```





