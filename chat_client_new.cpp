#include "my_tcp.h"
#include <iostream>
#include <string>
#include <thread>
using namespace std;

int main() {
    MyTcpClient client;
    string ip;
    int port = 9999;
    
    cout << "请输入服务器IP地址（直接回车则使用192.168.172.128）: ";
    getline(cin, ip);
    if (ip.empty()) ip = "192.168.172.128";
    
    if (!client.connectTo(ip, port)) {
        cerr << "连接服务器失败" << endl;
        return 1;
    }
    
    cout << "已连接到服务器，请输入你的用户名: ";
    string username;
    getline(cin, username);
    client.sendMessage(username);  // 先发送用户名
    
    // 启动接收线程
    client.setOnMessage([](const string& msg) {
        cout << msg << endl;
    });
    client.startReceiving();
    
    // 主循环：读取用户输入并发送
    string input;
    while (true) {
        getline(cin, input);
        if (input == "quit" || input == "QUIT") {
            break;
        }
        client.sendMessage(input);
    }
    
    return 0;
}
