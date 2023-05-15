//
// Created by beida on 2023/5/13.
//

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>


#define BUF_SIZE 9000


int socket(int argc,char* argv[]) {
//    if (argc <= 2){
//        printf("usage:%s ip_server_address port_number\n", basename(argv[0]));
//        return 1;
//    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in server_address{};
    bzero(&server_address,sizeof (server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&server_address.sin_addr);
    //htons  将一个无符号短整型数值转换为网络字节序，即大端模式(big-endian)
    server_address.sin_port = htons(port);
    int sock_fd = socket(PF_INET,SOCK_STREAM,0);
    assert(sock_fd >= 0);

    int recv_buf = 50;
    int len = sizeof (recv_buf);
    //先设置TCP接收缓冲区的大小，然后立即读取数据
    setsockopt(sock_fd,SOL_SOCKET,SO_SNDBUF,&recv_buf,len);
    getsockopt(sock_fd,SOL_SOCKET,SO_SNDBUF,&recv_buf,(socklen_t*)&len);
    printf("the tcp receive buffer size after setting is %d\n", recv_buf);



    int ret = bind(sock_fd,(struct sockaddr*)&server_address,sizeof (server_address));
    assert(ret != -1);

    ret = listen(sock_fd,5);
    assert(ret != -1);

    struct sockaddr_in client{};
    socklen_t client_addrlength = sizeof (client);
    int connfd = accept(sock_fd,(struct sockaddr*)&client,&client_addrlength);
    if (connfd < 0){
        printf("errno is:%d\n",errno);
    } else{
        char buffer[BUF_SIZE];
        memset(buffer,'\0',BUF_SIZE);
        while (recv(connfd,buffer,BUF_SIZE-1,0)>0){}
        close(connfd);

    }

    close(sock_fd);
    return 0;
}
int  main(){
    char*argc[4];
    argc[0]="dd";argc[1]="0.0.0.0";argc[2]="12345";argc[3]="256";
    socket(5,argc);
}