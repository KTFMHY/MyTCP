#include "my_tcp.h"
#include <iostream>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <cerrno>

// ========== MyTcpServer 实现 ==========
MyTcpServer::MyTcpServer() : m_serverFd(-1), m_epfd(-1), m_port(0), m_running(false) {}

MyTcpServer::~MyTcpServer() {
    stop();
}

bool MyTcpServer::start(int port) {
    if (m_running) return false;
    m_port = port;
    
    m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) {
        perror("socket failed");
        return false;
    }
    
    int opt = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(m_serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(m_serverFd);
        return false;
    }
    
    if (listen(m_serverFd, 10) < 0) {
        perror("listen failed");
        close(m_serverFd);
        return false;
    }
    
    m_epfd = epoll_create1(0);
    if (m_epfd < 0) {
        perror("epoll_create1 failed");
        close(m_serverFd);
        return false;
    }
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = m_serverFd;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_serverFd, &ev) < 0) {
        perror("epoll_ctl add server failed");
        close(m_epfd);
        close(m_serverFd);
        return false;
    }
    
    m_running = true;
    m_thread = std::thread(&MyTcpServer::run, this);
    std::cout << "MyTcpServer started on port " << port << std::endl;
    return true;
}

void MyTcpServer::stop() {
    m_running = false;
    if (m_serverFd >= 0) {
        close(m_serverFd);
        m_serverFd = -1;
    }
    if (m_epfd >= 0) {
        close(m_epfd);
        m_epfd = -1;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void MyTcpServer::setOnMessage(std::function<void(int, const std::string&)> callback) {
    m_onMessage = callback;
}

bool MyTcpServer::sendToClient(int clientFd, const std::string& msg) {
    if (clientFd < 0) return false;
    int ret = send(clientFd, msg.c_str(), msg.size(), 0);
    return ret > 0;
}

void MyTcpServer::broadcastToAll(const std::string& msg, int excludeFd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (int fd : m_clients) {
        if (fd != excludeFd) {
            send(fd, msg.c_str(), msg.size(), 0);
        }
    }
}

void MyTcpServer::run() {
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];
    
    while (m_running) {
        int n = epoll_wait(m_epfd, events, MAX_EVENTS, 1000);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == m_serverFd) {
                handleNewConnection();
            } else {
                handleClientData(events[i].data.fd);
            }
        }
    }
}

void MyTcpServer::handleNewConnection() {
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(m_serverFd, (struct sockaddr*)&client_addr, &len);
    if (client_fd < 0) return;
    
    // 设置非阻塞
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    // 关键：将新客户端添加到 epoll 监听
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        perror("epoll_ctl add client failed");
        close(client_fd);
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clients.push_back(client_fd);
    }
    std::cout << "新客户端连接, fd=" << client_fd << std::endl;
}

void MyTcpServer::handleClientData(int client_fd) {
    char buffer[1024];
    int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv error");
            close(client_fd);
            epoll_ctl(m_epfd, EPOLL_CTL_DEL, client_fd, nullptr);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), client_fd), m_clients.end());
        }
        return;
    }
    if (n == 0) {
        close(client_fd);
        epoll_ctl(m_epfd, EPOLL_CTL_DEL, client_fd, nullptr);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), client_fd), m_clients.end());
        std::cout << "客户端断开, fd=" << client_fd << std::endl;
        return;
    }
    buffer[n] = '\0';
    std::string msg(buffer);
    std::cout << "[DEBUG] handleClientData 收到消息: " << msg << std::endl;
    if (m_onMessage) {
        m_onMessage(client_fd, msg);
    }
}

// ========== MyTcpClient 实现 ==========
MyTcpClient::MyTcpClient() : m_sock(-1), m_connected(false) {}

MyTcpClient::~MyTcpClient() {
    disconnect();
}

bool MyTcpClient::connectTo(const std::string& ip, int port) {
    m_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock < 0) return false;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        close(m_sock);
        return false;
    }
    
    if (connect(m_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(m_sock);
        return false;
    }
    
    m_connected = true;
    return true;
}

void MyTcpClient::disconnect() {
    if (m_connected) {
        close(m_sock);
        m_sock = -1;
        m_connected = false;
        if (m_recvThread.joinable()) m_recvThread.join();
    }
}

bool MyTcpClient::sendMessage(const std::string& msg) {
    if (!m_connected) return false;
    int ret = send(m_sock, msg.c_str(), msg.size(), 0);
    return ret > 0;
}

void MyTcpClient::setOnMessage(std::function<void(const std::string&)> callback) {
    m_onMessage = callback;
}

void MyTcpClient::startReceiving() {
    if (m_connected) {
        m_recvThread = std::thread(&MyTcpClient::receiveLoop, this);
    }
}

void MyTcpClient::receiveLoop() {
    char buffer[1024];
    while (m_connected) {
        int n = recv(m_sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            std::cout << "服务器断开连接" << std::endl;
            break;
        }
        buffer[n] = '\0';
        if (m_onMessage) {
            m_onMessage(std::string(buffer));
        }
    }
    m_connected = false;
}
