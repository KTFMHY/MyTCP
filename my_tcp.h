#ifndef MY_TCP_H
#define MY_TCP_H

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET SOCKET_T;
    #define INVALID_SOCKET_T INVALID_SOCKET
    #define SOCKET_T_CLOSE(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cstring>
    typedef int SOCKET_T;
    #define INVALID_SOCKET_T (-1)
    #define SOCKET_T_CLOSE(s) close(s)
#endif

#include <string>
#include <functional>
#include <thread>
#include <vector>
#include <mutex>

class MyTcpServer {
public:
    MyTcpServer();
    ~MyTcpServer();
    
    bool start(int port);
    void stop();
    void setOnMessage(std::function<void(int, const std::string&)> callback);
    bool sendToClient(int clientFd, const std::string& msg);
    void broadcastToAll(const std::string& msg, int excludeFd = -1);
    
private:
    void run();
    void handleNewConnection();
    void handleClientData(int clientFd);
    
    int m_serverFd;
    int m_epfd;          // epoll 实例描述符
    int m_port;
    bool m_running;
    std::thread m_thread;
    std::vector<int> m_clients;
    std::mutex m_mutex;
    std::function<void(int, const std::string&)> m_onMessage;
};

class MyTcpClient {
public:
    MyTcpClient();
    ~MyTcpClient();
    
    bool connectTo(const std::string& ip, int port);
    void disconnect();
    bool sendMessage(const std::string& msg);
    void setOnMessage(std::function<void(const std::string&)> callback);
    void startReceiving();
    
private:
    void receiveLoop();
    
    SOCKET_T m_sock;
    bool m_connected;
    std::thread m_recvThread;
    std::function<void(const std::string&)> m_onMessage;
};

#endif
