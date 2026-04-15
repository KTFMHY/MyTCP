#include "my_tcp.h"
#include <iostream>
#include <string>
#include <map>
#include <mutex>
using namespace std;

map<int, string> clientNames;
mutex nameMutex;

int main() {
    MyTcpServer server;
    
    server.setOnMessage([&](int clientFd, const string& msg) {
        // 1. 判断是否已经设置用户名
        cout << "[DEBUG] 收到来自 fd=" << clientFd << " 的消息: " << msg << endl;
        bool hasName = false;
        {
            lock_guard<mutex> lock(nameMutex);
            hasName = (clientNames.find(clientFd) != clientNames.end());
        }
        
        // 2. 如果没有用户名，则把这条消息作为用户名
        if (!hasName) {
            {
                lock_guard<mutex> lock(nameMutex);
                clientNames[clientFd] = msg;
            }
            cout << "客户端 " << clientFd << " 设置用户名为: " << msg << endl;
            // 通知其他客户端有新用户加入
            string joinMsg = "[系统] " + msg + " 加入了聊天室";
            server.broadcastToAll(joinMsg, clientFd);
            // 单独回复该客户端
            server.sendToClient(clientFd, "[系统] 欢迎 " + msg + "，你可以开始聊天了");
            return;
        }
        
        // 3. 已有用户名，广播消息
        string name;
        {
            lock_guard<mutex> lock(nameMutex);
            name = clientNames[clientFd];
        }
        string fullMsg = "[" + name + "]: " + msg;
        // 广播给所有其他客户端（不包括自己）
        server.broadcastToAll(fullMsg, clientFd);
        // 同时回显给发送者自己（让发送者也能看到自己的消息）
        server.sendToClient(clientFd, fullMsg);
        // 服务器控制台也打印
        cout << fullMsg << endl;
    });
    
    if (!server.start(9999)) {
        cerr << "服务器启动失败" << endl;
        return 1;
    }
    
    cout << "新聊天室服务器已启动，按回车键停止..." << endl;
    cin.get();
    server.stop();
    return 0;
}
