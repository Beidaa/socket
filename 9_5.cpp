//
// Created by beida on 2023/5/17.
//
#include <sys/socket.h>
#include <sys/types.h>
#include <cassert>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <cerrno>

int setnonblocking(int fd) {
    // 获取文件描述符的旧标志位
    int oldopt = fcntl(fd, F_GETFL);
    // 将 O_NONBLOCK 标志位或运算到旧标志位上，设置为非阻塞模式
    int newopt = oldopt | O_NONBLOCK;
    // 将新标志位设置到该文件描述符上
    fcntl(fd, F_SETFL, newopt);
    // 返回旧的文件状态标志
    return oldopt;
}

int unblock_connect(const char* ip, int port, int time) {
    // 设置服务端地址结构体
    struct sockaddr_in address;
    address.sin_port = htons(port);
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);

    // 创建套接字
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    // 将套接字设置为非阻塞模式
    int oldsockopt = setnonblocking(sockfd);

    // 连接远程主机
    int ret = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    // 若立即连接成功，则返回 sockfd
    if(ret == 0) {
        printf("connect with server immediately\n");
        // 将套接字设置回旧标志
        fcntl(sockfd, F_SETFL, oldsockopt);
        return sockfd;
    } else if(errno != EINPROGRESS) { // 连接失败，且不是因为手动设置为非阻塞导致的，返回错误
        printf("unblock connect not support\n");
        return -1;
    }
    // 等待连接完成，使用 select() 监听可写事件
    fd_set readfds;
    fd_set writefds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(sockfd, &writefds);

    timeout.tv_sec = time;
    timeout.tv_usec = 0;

    ret = select(sockfd + 1, nullptr, &writefds, nullptr, &timeout);
    // select() 出错或者超时，返回错误
    if(ret <= 0) {
        printf("connect time error\n");
        close(sockfd);
        return -1;
    }
    // 可写事件未发生，返回错误
    if(!FD_ISSET(sockfd, &writefds)) {
        printf("no events on sockfd found\n");
        close(sockfd);
        return -1;
    }

    int error = 0;
    socklen_t length = sizeof(error);

    // 获取套接字错误码
    if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &length) < 0) {
        printf("get socket option failed\n");
        close(sockfd);
        return -1;
    }

    // 连接失败，返回错误
    if(error != 0) {
        printf("connection failed after select with the error: %d \n", error);
        close(sockfd);
        return -1;
    }

    // 连接成功，将套接字设置回旧标志
    printf("connect ready after select with the socket: %d \n", sockfd);
    fcntl(sockfd, F_SETFL, oldsockopt);
    return sockfd;
}

int main(int argc, char* argv[]) {
    const char* ip = "172.18.0.2";
//    const char* ip = "127.0.0.1";
    int port = atoi("1235");

    int sockfd = unblock_connect(ip, port, 10);
    int sockfd2 = unblock_connect(ip, port, 10);
    char* buff="cddk";
    char* buff2="cdddsdsdsdsk";
    send(sockfd,buff,strlen (buff),0);
    sleep(3);
    send(sockfd2,buff2,strlen (buff2),0);
//    if(sockfd < 0) {
//        return 1;
//    }

    return 0;
}
