#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <csignal>
#include <cassert>
#include <unistd.h>



static bool stop = false;
static void handle_term(int sig){
    stop = true;
}
int socket(int argc,char* argv[]) {
    //参考链接 https://blog.51cto.com/u_15284125/2988992
    //signal函数 第一参数是指需要进行处理的信号，第二个参数是指 处理的方式(系统默认/忽略/捕获)
    //SIGTERM 请求中止进程，kill命令缺省发送 交给handle_term函数进行处理
    signal(SIGTERM,handle_term);
//    if (argc < 3){
//        //basename 参考链接 https://blog.csdn.net/Draven_Liu/article/details/38235585
//        //假设路径为 nihao/nihao/jhhh/txt.c
//        //basename函数并不会关心路径是否正确，文件是否存在，只不过是把路径上除了最后的txt.c 这个文件名字其他的东西都删除了然后返回txt.c而已
//        std::cout << "usage:" <<basename(argv[0]) << "ip_address port_number backlog\n"<<std::endl;
//    }
    //argv[1] ip地址
    //argv[2] 端口号
    //argv[3] 日志级别
    const char* ip = argv[1];
    //atoi 把字符串转换成一个整数
    //参考链接 https://www.runoob.com/cprogramming/c-function-atoi.html
    int port = atoi(argv[2]);
    int backlog = atoi(argv[3]);
    //socket编程 第一个参数表示使用哪个底层协议族，对 TCP/IP协议族而言，该参数应该设置为PF_INET
    //对 TCP/IP协议族而言，其值取SOCK_STREAM表示传输层使用TCP协议
    //第三个参数是在前两个参数构成的协议集合下，再选择一个具体的协议  设置为0 ,表示使用默认协议
    int sock = socket(PF_INET,SOCK_STREAM,0);
    //断言 如果不正确 不会往下继续执行
    assert(sock>=0);
    //创建一个IPv4 socket地址
    //TCP/IP 协议族sockaddr_in 表示IPv4专用socket地址结构体
    struct sockaddr_in address;
    // bzero() 会将内存块（字符串）的前n个字节清零;
    // s为内存（字符串）指针，n 为需要清零的字节数。
    // 在网络编程中会经常用到。
//    bzero(&address,sizeof (address));
//    address.sin_family = AF_INET;
//    //int inet_pton(int af,const char* src,void* dst)
//    //af 指定地址族 AF_INET 或者 AF_INET6
//    //inet_pton函数成功返回1 失败返回0,并且设置errno
//    //errno 表示各种错误
//    // inet_pton 将字符串表示的IP地址src(使用点分十进制表示的IPv4地址和使用十六进制表示的IPv6)转换成网络字节序整数表示的IP地址,并把转换的结果存储在dst指向的内存中
    inet_pton(AF_INET,"172.18.0.2",&address.sin_addr);
//    //const char* inet_ntop(int af,const char* src,void* dst,socklen_t cnt)
//    //inet_tpon函数和inet_pton进行相反的转换，前三个参数的含义与其相同，最后一个参数cnt指定目标存储单元的大小
//    //成功 返回目标单元的地址 失败返回NULL 并且设置errno
//    address.sin_port = htons(port);
    //address.sin_addr.s_addr=INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port=htons(port);
    //bind将 my_addr所指的socket地址(2)分配给未命名的sockfd(1)文件描述符，addrlen参数(3)指出该socket地址的长度。
    int ret = bind(sock,(struct sockaddr*)&address,sizeof (address));
    assert(ret != -1);
    listen(sock,5);
    //若仅仅设置backlog则客户端断开连接服务器也不知道其断开连接
    //循环等待连接 直到有SIGTERM信号将其中断
    while(!stop){
        sleep(1);
    }
    //关闭 socket
    close(sock);
    return 0;
}
int  main(){
    char*argc[4];
    argc[0]="dd";argc[1]="192.168.1.109";argc[2]="12345";argc[3]="5";
    socket(1,argc);
}