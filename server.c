
#define _GNU_SOURCE 1

#include <sys/types.h> // 包含系统类型定义的头文件
#include <sys/socket.h> // 包含套接字相关的头文件
#include <errno.h> // 包含错误处理相关的头文件
#include <string.h> // 包含字符串处理相关的头文件
#include <netinet/in.h> // 包含网络地址相关的头文件
#include <arpa/inet.h> // 包含IP地址转换相关的头文件
#include <stdio.h> // 包含标准输入输出相关的头文件
#include <unistd.h> // 包含Unix标准函数相关的头文件
#include <assert.h> // 包含断言相关的头文件
#include <stdlib.h> // 包含标准库函数相关的头文件
#include <fcntl.h> // 包含文件控制相关的头文件

#define BUFFER_SIZE 1024
void distributeMessage(int sock_client, char* msg); // 声明一个函数，用于向其他客户端分发消息
int accountProcess(char* msg, int msg_socket); // 声明一个函数，用于处理账户登录或注册的消息
void processMessage(char* msg, int msg_socket); // 声明一个函数，用于处理客户端发送的消息

int accountLogin(char* username, char* password, int msg_socket); // 声明一个函数，用于验证账户登录是否成功
void accountCreate(char* username, char* password); // 声明一个函数，用于创建新账户

struct user { // 定义一个结构体，表示用户信息
    char nikename[10]; // 用户昵称，最多10个字符
    char username[10]; // 用户名，最多10个字符
    char password[16]; // 密码，最多16个字符
    int status; // 用户状态，0表示离线，1表示在线
};

struct online_user { // 定义一个结构体，表示在线用户信息
    char nikename[10]; // 用户昵称，最多10个字符
    int sockfd; // 用户对应的套接字描述符
    int status; // 用户状态，0表示离线，1表示在线
};

struct user users[100]; // 定义一个数组，存储最多100个用户信息
struct online_user online_users[100]; // 定义一个数组，存储最多100个在线用户信息
int sum_user = -1; // 定义一个变量，表示当前用户数量，默认为-1
int sum_online_user = -1; // 定义一个变量，表示当前在线用户数量，默认为-1

int main(int argc, char* argv[]) { // 定义主函数

    if(argc <= 2) { // 如果参数数量小于等于2（即没有指定IP地址和端口号）
        printf("Wrong number of parameters!\n"); // 打印错误信息
        return 1; // 返回1表示异常退出
    }

    char* ip = argv[1]; // 获取第一个参数作为IP地址
    int port = atoi(argv[2]); // 获取第二个参数作为端口号，并转换为整数类型
    struct sockaddr_in address_server, address_client; // 定义两个结构体变量，分别表示服务器和客户端的地址信息
    memset(&address_server, 0, sizeof(address_server)); // 将服务器地址信息初始化为0
    address_server.sin_family = AF_INET; // 设置服务器地址信息的协议族为IPv4
    address_server.sin_port = htons(port); // 设置服务器地址信息的端口号，并转换为网络字节序
    inet_pton(AF_INET, ip, &address_server.sin_addr); // 将IP地址从字符串格式转换为二进制格式，并存储到服务器地址信息中

    int sock_server = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock_server >= 0);
    int ret = bind(sock_server, (struct sockaddr*)&address_server, sizeof(address_server));
    assert(ret != -1);
    ret = listen(sock_server, 5);
    assert(ret != -1);

    fd_set readfds, testfds;

    FD_ZERO(&readfds);
    FD_SET(sock_server, &readfds);

    while(1) {

        testfds = readfds;
        int result_select = select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, (struct timeval*)0);
        if(result_select < 1) {
            perror("server error\n");
            exit(1);
        }

        int count_fd;
        for(count_fd = 0; count_fd < FD_SETSIZE; count_fd++) {
            if(FD_ISSET(count_fd, &testfds)) {

                if(count_fd == sock_server) {
                    int len_client = sizeof(address_client);
                    int sock_client = accept(sock_server, (struct sockaddr*)&address_client, &len_client);
                    FD_SET(sock_client, &readfds);
                    printf("add a new client to readfds %d\n", sock_client);
                } else {

                    int pipefd[2];
                    int result_pipe = pipe(pipefd);

                    char msg[BUFFER_SIZE];
                    memset(msg, '\0', BUFFER_SIZE);
                    result_pipe = splice(count_fd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE | SPLICE_F_NONBLOCK);

                    if(result_pipe == -1) {
                        close(count_fd);
						// �ͻ��˵��ߺ�����ͻ��� 
                        for(int i =0; i<= sum_online_user; i++) {
                            
                            if(online_users[i].sockfd == i) {
                                online_users[i].status = 0;
                            }
                        }
                    }

                    read(pipefd[0], msg, BUFFER_SIZE - 1);

                    processMessage(msg, count_fd);
                }
            }
        }
    }
    printf("close socket\n");
    close(sock_server);
    return 0;
}

#define LOGIN_FLAG 1
#define SENDMSG_FLAG 2

#define LOGIN_SUCCESS 1
#define LOGIN_PASSWORD_WRONG 2
#define LOGIN_SUCCESS_NEW_USER 3
#define LOGIN_ERROR_LIGINED 4
void processMessage(char* msg, int msg_socket) {
    if(msg[0] == '{' && msg[2] == '@' && msg[strlen(msg) - 3] == '}') {

        if(msg[1] ==  48 + LOGIN_FLAG) { // ����Ϊ��½
            
            int result = accountProcess(msg + 3, msg_socket);

            char resu[1024];
            memset(resu, '\0', 1024);

            if(result == LOGIN_SUCCESS) {
                strcat(resu, "{6@LOGIN_SUCCESS}\r\n");
            } else if(result == LOGIN_SUCCESS_NEW_USER) {
                strcat(resu, "{6@LOGIN_SUCCESS_NEW_USER PLEASE LOGIN IN}\r\n");
            } else if(result == LOGIN_PASSWORD_WRONG) {
                strcat(resu, "{6@LOGIN_PASSWORD_WRONG}\r\n");
            } else if(result == LOGIN_ERROR_LIGINED) {
                strcat(resu, "{6@DON`t LOGIN AGAIN}\r\n");
            }

            send(msg_socket, resu, strlen(resu), 0);
            
        } else if(msg[1] == 48 + SENDMSG_FLAG){ // ����Ϊ������Ϣ
            distributeMessage(msg_socket, msg);
        }
    }
}

// ��msg�л�ȡ�˺����� ���ַ�����½����ע��
int accountProcess(char* msg, int msg_socket) {
    char username[10];

    char password[16];
    int account_length = 0;
    int is_username = 1;
    for(int i = 0; i < strlen(msg) - 3; i++) {
        if(is_username) {

            if(msg[i] == '@') {
                is_username = 0;
                account_length = 0;
                continue;
            }

            username[account_length++] = msg[i];

        } else {
            password[account_length++] = msg[i];
        }
    }

    int login_result = accountLogin(username, password, msg_socket);
    if(login_result != 0) {
        return login_result;
    }

    accountCreate(username, password);

    return LOGIN_SUCCESS_NEW_USER;
}

int accountLogin(char* username, char* password, int msg_socket) {
    for(int i = 0; i <= sum_user; i++) {

        if(!strcmp(users[i].username, username)) {
            if(!strcmp(users[i].password, password)) {
                // login success

                for(int relogin_test_count = 0; relogin_test_count <= sum_online_user; relogin_test_count++) {

                    if(online_users[relogin_test_count].sockfd == msg_socket) {
                        return LOGIN_ERROR_LIGINED;
                    }
                }

                sum_online_user++;
                strcpy(online_users[sum_online_user].nikename, users[i].nikename);
                online_users[sum_online_user].status = 1;
                online_users[sum_online_user].sockfd = msg_socket;
                return LOGIN_SUCCESS;
            } else {
                return LOGIN_PASSWORD_WRONG;
            }
        }
    }

    return 0;
}

void accountCreate(char* username, char* password) {
    sum_user++;

    struct user new_user;
    memset(&new_user, 0, sizeof(new_user));
    strcpy(new_user.username, username);
    strcpy(new_user.password, password);

    char nikename[10];
    memset(nikename, '\0', 10);
    strcat(nikename, "HJKSDK");
    nikename[strlen(nikename)] = 48 + sum_user;
    strcpy(new_user.nikename, nikename);

    users[sum_user] = new_user;
}

void distributeMessage(int sock_client, char* msg) {
    char* recvMsg = "{6@ok}\r\n";

    for(int i = 0; i <= sum_online_user; i++) {
        struct online_user the_user = online_users[i];
        if(the_user.sockfd != sock_client && the_user.status == 1) {
            char ret_msg[1024];
            memset(ret_msg, '\0', 1024);
            strcat(ret_msg, "from ");
            strcat(ret_msg, the_user.nikename);
            strcat(ret_msg, " :");
            strcat(ret_msg, msg);

            send(the_user.sockfd, ret_msg, strlen(ret_msg), 0);
        }
    }

    send(sock_client, recvMsg, strlen(recvMsg), 0);
}
