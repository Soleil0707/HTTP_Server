# HTTP_Server
使用Libevent、Openssl等库实现的HTTP服务器

# 功能
支持以下操作：
* GET请求从服务器下载文件
* POST上传表单以及小文件
* HTTPS
* 多路并发
* 分块传输
* 持久连接

# 编译方式
```shell
make
```

# 使用方法
```shell
./HTTP_SERVER.ELF -p [端口号] 服务器资源目录
```
