//
// Created by beida on 2023/5/18.
//

#include <sys/socket.h>
#include <sys/types.h>
#include <cassert>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

#define BUFFER_SIZE 64

//主函数
int main(int argc, char* argv[]) {
    //检查参数数量是否正确
//    if(argc <= 2) {
//        printf("Wrong number of parameters \n");
//        return 1;
//    }

    //获取服务器端口号和IP地址
    int port = atoi("12343");
    char* ip = "172.18.0.2";

    //创建sockaddr_in结构体并初始化
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    //创建套接字
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    //连接服务器
    if(connect(sockfd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        printf("Connect error\n");
        close(sockfd);
        return 1;
    }

    //使用poll函数进行IO多路复用
    pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;

    //定义读缓冲区和管道
    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);

    //循环等待事件发生
    while (1) {

        //监听fds数组中的文件描述符并等待事件发生
        ret = poll(fds, 2, -1);
        if(ret < 0) {
            printf("poll failure\n");
            break;
        }

        //如果服务器关闭连接，则退出循环
        if(fds[1].revents & POLLRDHUP) {
            printf("server close the connection\n");
            break;
        }

        //如果服务器有数据可读，则将数据读取到缓冲区并输出
        if(fds[1].revents & POLLIN) {
            memset(read_buf, '\0', BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE - 1, 0);
            printf("%s\n", read_buf);
        }

        //如果标准输入有数据可读，则将数据写入管道
        if(fds[0].revents & POLLIN) {
            ret = splice(0, nullptr, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            ret = splice(pipefd[0], nullptr, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
//            sleep(1);
//            break;
        }
    }

    //关闭套接字和管道
    close(sockfd);
    return 0;
}

