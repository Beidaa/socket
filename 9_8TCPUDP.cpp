//
// Created by beida on 2023/5/18.
//

#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <netinet/in.h>

#include <cstdlib>

#include <fcntl.h>



#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

static int stop = 0; // 是否停止标志

// SIGTERM信号处理函数，用于结束主程序中的循环
static void handle_term(int sig) {
    stop = 1;
}

// 设置文件描述符为非阻塞模式
static int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 添加事件到红黑树epollfd中，并设置EPOLLONESHOT事件
static void addfd(int epollfd, int fd, int enable_et) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (enable_et) {
        event.events |= EPOLLET;
    }
    // 设置EPOLLONESHOT事件
    event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd); // 设置套接字为非阻塞模式
}

// 从红黑树epollfd中移除fd事件
static void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改文件描述符fd上的事件为events
static void modfd(int epollfd, int fd, int events) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = events | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 读取客户端数据并返回应答
static void do_read(int epollfd, int fd, char *buf, int buffer_size, int is_et) {
    int nread;
    // 循环读取客户端发送来的数据，直到数据读完或EAGAIN出现
    while (1) {
        if (is_et) { // 边缘触发模式
            nread = recv(fd, buf, buffer_size - 1, MSG_DONTWAIT);
            if (nread < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 读取完毕
                    modfd(epollfd, fd, EPOLLIN);
                    break;
                }
                removefd(epollfd, fd);
                break;
            } else if (nread == 0) {
                removefd(epollfd, fd);
                break;
            }
        } else { // 水平触发模式
            nread = recv(fd, buf, buffer_size - 1, 0);
            if (nread < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 读取完毕
                    modfd(epollfd, fd, EPOLLIN);
                    break;
                }
                removefd(epollfd, fd);
                break;
            } else if (nread == 0) {
                removefd(epollfd, fd);
                break;
            }
        }
        buf[nread] = '\0';
        printf("read %d bytes: %s\n", nread, buf);

        // 将客户端发送来的数据返回给客户端
        send(fd, buf, strlen(buf), 0);
    }
}

// 处理UDP服务
static void do_udp_service(int sockfd, struct sockaddr_in *client_addr, socklen_t client_addrlen) {
    char buf[UDP_BUFFER_SIZE];
    memset(buf, '\0', UDP_BUFFER_SIZE);

    ssize_t nread = recvfrom(sockfd, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr *)client_addr, &client_addrlen);
    if (nread > 0) {
        printf("UDP Received %zd bytes from %s:%d: %s\n", nread,
            inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), buf);
        int nwrited = sendto(sockfd, buf, nread, 0, (struct sockaddr *)client_addr, client_addrlen);
        if (nwrited == -1) {
            perror("sendto error");
        }
        printf("UDP Sent %d bytes to %s:%d\n", nwrited, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
    } else if (nread == -1) {
        perror("recvfrom error");
    }
}

// 主函数，同时处理TCP和UDP请求
int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    // 创建TCP套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    // 绑定TCP套接字
    int ret = bind(sockfd, (struct sockaddr*)&address, sizeof(address));
    if (ret < 0) {
        perror("bind tcp socket error");
        return 1;
    }

    // 监听TCP套接字，并将其加入红黑树epollfd中
    ret = listen(sockfd, 5);
    if (ret < 0) {
        perror("listen error");
        return 1;
    }
    int epollfd = epoll_create(5);
    if (epollfd == -1) {
        perror("epoll_create error");
        return 1;
    }
    struct epoll_event events[MAX_EVENT_NUMBER];
    addfd(epollfd, sockfd, 1);

    // 创建UDP套接字
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    // 设置UDP套接字为非阻塞模式
    setnonblocking(udpfd);
    struct sockaddr_in udp_address;
    bzero(&udp_address, sizeof(udp_address));
    udp_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &udp_address.sin_addr);
    udp_address.sin_port = htons(port + 1);

    // 绑定UDP套接字，并将其加入红黑树epollfd中
    ret = bind(udpfd, (struct sockaddr*)&udp_address, sizeof(udp_address));
    if (ret < 0) {
        perror("bind udp socket error");
        return 1;
    }
    addfd(epollfd, udpfd, 1);

    // 设置SIGTERM信号的处理函数
    signal(SIGTERM, handle_term);

    while (!stop) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll_wait error\n");
            break;
        }
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == sockfd) { // 处理TCP连接请求
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(sockfd, (struct sockaddr*)&client_address, &client_addrlen);
                addfd(epollfd, connfd, 1); // 将新连接的套接字加入红黑树epollfd中
            } else if (sockfd == udpfd) { // 处理UDP请求
                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                do_udp_service(udpfd, &client_addr, client_addrlen);
            } else { // 处理已连接套接字上的读写事件
                char buf[TCP_BUFFER_SIZE];
                memset(buf, '\0', TCP_BUFFER_SIZE);
                if (events[i].events & EPOLLIN) { // 可读事件
                    // 判断是否为ET模式
                    bool is_et = (events[i].events & EPOLLET);
                    do_read(epollfd, sockfd, buf, TCP_BUFFER_SIZE, is_et);
                }
            }
        }
    }

    close(sockfd);
    close(udpfd);
    return 0;
}
