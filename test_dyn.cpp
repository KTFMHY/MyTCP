#include "my_tcp.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    MyTcpClient client;
    if (!client.connectTo("192.168.172.128", 9999)) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }
    std::cout << "连接成功，发送消息" << std::endl;
    client.sendMessage("Hello from dynamic library test");
    
    // 等待 2 秒，确保服务器处理完消息
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 可选：优雅断开连接（调用 disconnect 会关闭 socket）
    client.disconnect();
    std::cout << "测试完成" << std::endl;
    return 0;
}
