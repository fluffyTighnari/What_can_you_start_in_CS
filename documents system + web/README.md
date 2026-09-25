在web2的基础上：

支持静态资源全类型（利用响应头content-type）

实现CGI协议，能通过解析请求调用外部程序

实现TCP连接优化，SO\_REUSEADDR/SO\_KEEPALIVE/TCP\_NODELAY

实现信号处理优化，支持SIGINT、SIGPIPE

实现线程池，通过任务队列和预定义线程

实现I/O多路复用，通过epoll支持一个线程管理多个套接字

