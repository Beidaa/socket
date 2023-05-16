//
// Created by beida on 2023/5/16.
//
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <netdb.h>
#include <fcntl.h>//open库
#include <sys/uio.h>//redv库



//int main(int argc, char* argv[]) {
////    if (argc <= 2) {
////        printf("the parmerters is wrong\n");
////        exit(errno);
////    }
//    char *ip = "127.0.0.1";
//
//    int port = atoi("12376");
//    printf("the port is %d the ip is %s\n", port, ip);
//
//    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
//    assert(sockfd >= 0);
//
//    struct sockaddr_in address{};
//    address.sin_family = AF_INET;
//    address.sin_port = htons(port);
//    inet_pton(AF_INET, ip, &address.sin_addr);
//
//    int ret = bind(sockfd, (sockaddr *) &address, sizeof(address));
//    assert(ret != -1);
//
//    ret = listen(sockfd, 5);
//
//    int clientfd{};
//    sockaddr_in client_address{};
//    socklen_t client_addrlen = sizeof(client_address);
//
//    clientfd = accept(sockfd, (sockaddr *) &client_address, &client_addrlen);
//    if (clientfd < 0) {
//        printf("accept error\n");
//    } else {
//        printf("a new connection from %s:%d success\n", inet_ntoa(client_address.sin_addr),
//               ntohs(client_address.sin_port));
//        int fds[2];
//
//        //splice实现echo服务器
//        pipe(fds);
//        ret = splice(clientfd, nullptr, fds[1], nullptr, 32768, SPLICE_F_MORE);
//        assert(ret != -1);
//
//
//        ret = splice(fds[0], nullptr, clientfd, nullptr, 32768, SPLICE_F_MORE);
//        assert(ret != -1);
//
//
//
//        //
//        close(clientfd);
//    }
//    close(sockfd);
//    exit(0);
//}
int main(int fd){
    int old_option= fcntl(fd,F_GETFD);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}
