//
// Created by beida on 2023/5/13.
//
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <csignal>
#include <cassert>
#include <unistd.h>


int socket(int argc,char* argv[]) {
    if (argc <= 2){
        printf("usage:%s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address{};
    bzero(&address,sizeof (address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    //htons  将一个无符号短整型数值转换为网络字节序，即大端模式(big-endian)
    address.sin_port = htons(port);
    int sock = socket(PF_INET,SOCK_STREAM,0);
    assert(sock >= 0);
    int ret = bind(sock,(struct sockaddr*)&address,sizeof (address));
    assert(ret != -1);
    ret = listen(sock,5);
    //等待20秒 等待客户端连接和相关操作 (掉线/退出)完成
    sleep(20);
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof (client);
    int connfd = accept(sock,(struct sockaddr*)&client,&client_addrlength);
    if (connfd < 0){
        printf("errno is:%d\n",errno);
    } else{
        //接受连接成功 则打印客户端的IP地址和端口号
        char remote[INET_ADDRSTRLEN];
        printf("connected with ip:%s and port:%d\n",
               inet_ntop(AF_INET,&client.sin_addr,remote,INET_ADDRSTRLEN),
                // 将一个无符号短整形数从网络字节顺序转换为主机字节顺序
               ntohs(client.sin_port));
        close(connfd);
    }
    close(sock);
    return 0;
}



int  main(){
    char*argc[4];
    argc[0]="dd";argc[1]="172.18.0.2";argc[2]="12345";argc[3]="5";
    socket(5,argc);
}