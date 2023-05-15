#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char *argv[]) {
    struct hostent *hostinfo;
    if ((hostinfo = gethostbyname("localhost")) == NULL) {  // 获取本机主机信息
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    struct servent *servinfo;
    if ((servinfo = getservbyname("daytime", "tcp")) == NULL) {  // 获取 daytime 服务信息
        perror("getservbyname");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = servinfo->s_port;  // 端口号使用 daytime 服务的端口 13
    memcpy(&address.sin_addr, hostinfo->h_addr_list[0], hostinfo->h_length);

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {  // 创建套接字
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr*)&address, sizeof(address)) < 0) {  // 连接到本机的 13 号端口
        perror("connect");
        exit(EXIT_FAILURE);
    }

    char buffer[512];
    int n = read(sockfd, buffer, sizeof(buffer));  // 读取服务器传回的数据
    if (n < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    buffer[n] = '\0';

    printf("%s", buffer);  // 打印时间信息

    close(sockfd);  // 关闭套接字

    return 0;
}
