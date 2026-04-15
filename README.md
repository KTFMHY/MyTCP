# MyTCP
### 跨平台 TCP 通信库 (MyTCP)                              [C++] [Socket] [多线程] [动态库]

- 设计并实现了统一的 TCP 服务端/客户端 C++ 类，通过条件编译屏蔽 Linux (epoll) 和 Windows (Winsock) 的 API 差异。
- 封装了多线程收发、连接管理、消息广播等逻辑，提供 `MyTcpServer` / `MyTcpClient` 简洁接口。
- 分别生成 Linux (.so) 和 Windows (.dll) 动态库，验证了跨平台互通性：Ubuntu 服务端与 Windows 客户端成功通信。
- 解决了 Windows 下 Winsock 初始化、连接重置错误处理、MSYS2 编译环境配置等实际问题。
- 基于该库重构了多人聊天室，支持多客户端并发消息广播，作为库的示例应用。
