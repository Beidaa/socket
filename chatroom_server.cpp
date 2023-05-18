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
#include <cerrno>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

//定义了一个客户端数据结构体，包含地址和数据缓冲区
struct client_data {
    sockaddr_in address;
    char* write_buff;
    char buf[BUFFER_SIZE];
};

//设置文件描述符为非阻塞模式
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL); //获取原本文件描述符的状态标志
    int new_option = old_option | O_NONBLOCK; //加上O_NONBLOCK标志
    fcntl(fd, F_SETFL, new_option); //设置新的状态标志
    return old_option; //返回旧的状态标志
}

int main(int argc, char* argv[]) {

//    if(argc <= 2) {
//        printf("Wrong number of parameters \n"); //参数输入错误
//        return 1;
//    }

    int port = atoi("12343");
    char* ip = "172.18.0.2";

    struct sockaddr_in address; //定义IPv4套接字地址结构
    memset(&address, 0, sizeof(address)); //清空地址结构
    address.sin_family = AF_INET; //地址族
    address.sin_port = htons(port); //端口号转换为网络字节序
    inet_pton(AF_INET, ip, &address.sin_addr); //将点分十进制的IP地址转换为网络字节序

    int sockfd = socket(PF_INET, SOCK_STREAM, 0); //创建TCP连接套接字
    assert(sockfd >= 0); //若创建失败则退出程序
    int ret = bind(sockfd, (struct sockaddr*)&address, sizeof(address)); //绑定套接字
    assert(ret != -1); //若绑定失败则退出程序

    ret = listen(sockfd, 5); //开启监听
    assert(ret != -1); //若监听失败则退出程序

/*
* 创建一个用户结构体数组，长度为FD_LIMIT，并且对每个socket连接进行初始化
* 为每个socket创建相应的客户端数据结构体
* 初始化pollfd结构体数组
*/
    client_data* users = new client_data[FD_LIMIT]; //创建客户端数据结构数组
    pollfd fds[USER_LIMIT + 1]; //pollfd数组
    int user_count = 0; //已连接用户的数目
    for(int i = 0; i <= USER_LIMIT; i++) {
        fds[i].fd = -1; //文件描述符
        fds[i].events = 0; //等待的事件
    }

    fds[0].fd = sockfd; //第一个文件描述符为监听套接字
    fds[0].events = POLLIN | POLLERR; //第一个文件描述符关注POLLIN和POLLERR

    while(1) {

        ret = poll(fds, user_count + 1, -1); //使用poll等待事件发生
        /*
         * 在调用poll函数后，操作系统会将实际发生的事件类型写入pollfd结构体中的revents成员，
         */
        if(ret < 0) {
            printf("poll failure\n"); //如果发生错误，则提示错误信息
            break;
        }

        for(int i =0; i < user_count + 1; i++) {
            if((fds[i].fd == sockfd) && (fds[i].revents & POLLIN)) { //若是监听套接字发生POLLIN事件

                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(sockfd, (struct sockaddr *) &client_address, &client_addrlength); //接受连接请求

                if (connfd < 0) {
                    printf("errno is: %d\n", errno); //返回错误号
                    continue;
                }

                if (user_count >= USER_LIMIT) { //若已连接用户数达到最大值

                    const char *info = "to many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0); //将错误信息发送给客户端
                    close(connfd);
                    continue;
                }

                user_count++; //用户数加一
                users[connfd].address = client_address; //存储用户地址信息
                setnonblocking(connfd); //设置socket为非阻塞模式
                fds[user_count].fd = connfd; //将当前连接的文件描述符添加到pollfd数组中
                fds[user_count].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_count].revents = 0;
                printf("come a new client, now have %d users\n", user_count); //客户端连接成功

            } else if(fds[i].revents & POLLERR) { //若发生POLLERR事件

                printf("get an error from %d\n", fds[i].fd);
                char errors[100];
                memset(errors, '\0', 100);
                socklen_t length = sizeof(errors);

                if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0) {
                    printf("get socketopt failed\n"); //获取SOCKET的当前状态，若获取失败则输出错误信息
                }
                continue;
            } else if(fds[i].revents & POLLRDHUP) { //如果对端关闭连接或者发送了FIN，则会收到POLLRDHUP事件

                //将最后一个客户端数据复制到当前位置并删除最后一个数据
                users[fds[i].fd] = users[fds[user_count].fd];
                close(fds[i].fd); //关闭对应的文件描述符
                fds[i] = fds[user_count];
                i--;
                user_count--;
                printf("a client left\n"); //客户端断开连接

            } else if(fds[i].revents & POLLIN) { //若非监听套接字发生POLLIN事件

                int connfd = fds[i].fd; //获取文件描述符
                memset(users[connfd].buf, '\0', BUFFER_SIZE); //初始化缓冲区
                ret = recv(connfd, users[connfd].buf, BUFFER_SIZE - 1, 0); //从socket中读取数据

                printf("get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd);

                if(ret < 0) { //读取失败

                    if(errno != EAGAIN) { //若读取操作没有执行完毕，说明发生了错误

                        close(connfd);
                        users[fds[i].fd] = users[fds[user_count].fd];
                        fds[i] = fds[user_count];
                        i--;
                        user_count--;
                    }
                } else if(ret == 0) { //表明客户端关闭了连接

                } else {
                    for(int j = 1; j <= user_count; j++) { //遍历所有已连接用户

                        if(fds[j].fd == connfd) { //若当前处理的文件描述符j是来自当前客户端connfd的描述符，则跳过此次循环

                            continue;
                        }

                        fds[j].events |= ~POLLIN; //修改fds[j]的等待事件
                        fds[j].events |= POLLOUT; //设置POLLPOLLOUT事件
                        users[fds[j].fd].write_buff = users[connfd].buf; //将数据缓存到待写缓冲区中
                    }
                }

            } else if(fds[i].revents & POLLOUT) { //若发生POLLPOUT事件：可写事件

                int connfd = fds[i].fd;

                if(!users[connfd].write_buff) { //数据为空

                    continue;
                }

                ret =send(connfd, users[connfd].write_buff, strlen(users[connfd].write_buff), 0); //将待发送数据从待写缓冲区发送出去

                // 将等待在该事件上的read event重新挂到该fd上，等待下一段数据到来
                users[connfd].write_buff = nullptr; //已发送数据置为空
                fds[i].events |= ~POLLOUT; //取消关注POLLPOUT事件
                fds[i].events |= POLLIN; //关注POLLIN事件
            }
        }
    }

    delete[] users; //释放动态分配的内存
    close(sockfd); //关闭套接字
    return 0;

}
