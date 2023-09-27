# Socket Server

Lab 5 for computer networks

## 文件说明

- package.h: 自定义协议中相关常量及数据包的定义
- package.cpp: package.h中定义的操作的具体实现
- server.h: 服务端头文件
- server.cpp: 服务端实现

## TODO

- [x] package.h/cpp
  - [x] 数据结构设计
  - [x] 数据的序列化与反序列化
  - [x] 序列化时的缓冲区溢出检查
  - [x] 反序列化时的格式正确性检查
- [ ] server.h/cpp
  - [x] 服务端初始化与端口绑定
  - [x] 服务端主线程监听，循环accept
  - [x] 服务端子线程处理请求
  - [ ] 服务端子线程之间共享资源的互斥访问