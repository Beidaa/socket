

//C/S 模型
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

int server(int argc, char* argv[]) { // 定义主函数

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

    int sock_server = socket(PF_INET, SOCK_STREAM, 0); // 创建一个套接字，用于与客户端通信
    assert(sock_server >= 0); // 断言套接字创建成功
    int ret = bind(sock_server, (struct sockaddr*)&address_server, sizeof(address_server)); // 将套接字绑定到服务器地址信息上
    assert(ret != -1); // 断言绑定成功
    ret = listen(sock_server, 5); // 让套接字进入监听状态，等待客户端的连接请求，最多允许5个连接请求排队
    assert(ret != -1); // 断言监听成功

    fd_set readfds, testfds; // 定义两个文件描述符集合，用于存储需要读取的套接字

    FD_ZERO(&readfds); // 将readfds集合清空
    FD_SET(sock_server, &readfds); // 将服务器套接字加入到readfds集合中

    while(1) { // 循环执行以下操作

        testfds = readfds; // 将testfds集合设置为readfds集合的副本
        int result_select = select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, (struct timeval*)0);
        // 调用select函数，检查testfds集合中的套接字是否有可读数据，如果有则返回可读套接字的数量，如果没有则阻塞等待，如果出错则返回-1
        if(result_select < 1) { // 如果select函数返回值小于1，表示出错或无可读套接字
            perror("server error\n"); // 打印错误信息
            exit(1); // 异常退出
        }

        int count_fd; // 定义一个循环变量，用于遍历所有可能的文件描述符
        for(count_fd = 0; count_fd < FD_SETSIZE; count_fd++) { // 遍历所有可能的文件描述符
            if(FD_ISSET(count_fd, &testfds)) { // 如果某个文件描述符在testfds集合中

                if(count_fd == sock_server) { // 如果该文件描述符是服务器套接字，表示有新的客户端连接请求
                    int len_client = sizeof(address_client); // 定义一个变量，用于存储客户端地址信息的长度
                    int sock_client = accept(sock_server, (struct sockaddr*)&address_client, &len_client);
                    // 接受客户端的连接请求，并返回一个新的套接字，用于与该客户端通信，并将客户端的地址信息存储到address_client变量中
                    FD_SET(sock_client, &readfds); // 将新的套接字加入到read
                    printf("add a new client to readfds %d\n", sock_client); // 打印添加新客户端的信息
                } else { // 如果该文件描述符不是服务器套接字，表示有客户端发送数据

                    int pipefd[2]; // 定义一个管道，用于在两个文件描述符之间传输数据
                    int result_pipe = pipe(pipefd); // 创建一个管道，并返回0表示成功，-1表示失败

                    char msg[BUFFER_SIZE]; // 定义一个变量，用于存储客户端发送的消息
                    memset(msg, '\0', BUFFER_SIZE); // 将消息初始化为0
                    result_pipe = splice(count_fd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
                    // 将客户端套接字的数据拷贝到管道的写端，不阻塞，返回拷贝的字节数

                    if(result_pipe == -1) { // 如果拷贝失败
                        close(count_fd); // 关闭客户端套接字

                        for(int i =0; i<= sum_online_user; i++) { // 遍历所有在线用户

                            if(online_users[i].sockfd == i) { // 如果找到对应的用户
                                online_users[i].status = 0; // 将用户的状态设置为离线
                            }
                        }
                    }

                    read(pipefd[0], msg, BUFFER_SIZE - 1); // 从管道的读端读取数据，并存储到msg变量中

                    processMessage(msg, count_fd); // 调用processMessage函数，处理客户端发送的消息
                }
            }
        }
    }

    printf("close socket\n"); // 打印关闭套接字的信息
    close(sock_server); // 关闭服务器套接字
    return 0; // 返回0表示正常退出
}

#define LOGIN_FLAG 1 // 定义一个宏，表示登录的标志
#define SENDMSG_FLAG 2 // 定义一个宏，表示发送消息的标志

#define LOGIN_SUCCESS 1 // 定义一个宏，表示登录成功的状态
#define LOGIN_PASSWORD_WRONG 2 // 定义一个宏，表示登录密码错误的状态
#define LOGIN_SUCCESS_NEW_USER 3 // 定义一个宏，表示登录成功并创建新用户的状态
#define LOGIN_ERROR_LIGINED 4 // 定义一个宏，表示登录错误已经在线的状态

void processMessage(char* msg, int msg_socket) { // 定义一个函数，用于处理客户端发送的消息，参数为消息内容和客户端套接字
    if(msg[0] == '{' && msg[2] == '@' && msg[strlen(msg) - 3] == '}') { // 如果消息的格式符合要求

        if(msg[1] == 48 + LOGIN_FLAG) { // 如果消息为登录

            int result = accountProcess(msg + 3, msg_socket); // 调用accountProcess函数，处理登录请求，并返回结果

            char resu[1024]; // 定义一个变量，用于存储返回给客户端的结果
            memset(resu, '\0', 1024); // 将结果初始化为0

            if(result == LOGIN_SUCCESS) { // 如果登录成功
                strcat(resu, "{6@LOGIN_SUCCESS}\r\n"); // 将结果拼接为"{6@LOGIN_SUCCESS}\r\n"
            } else if(result == LOGIN_SUCCESS_NEW_USER) { // 如果登录成功并创建新用户
                strcat(resu, "{6@LOGIN_SUCCESS_NEW_USER PLEASE LOGIN IN}\r\n"); // 将结果拼接为"{6@LOGIN_SUCCESS_NEW_USER PLEASE LOGIN IN}\r\n"
            } else if(result == LOGIN_PASSWORD_WRONG) { // 如果登录密码错误
                strcat(resu, "{6@LOGIN_PASSWORD_WRONG}\r\n"); // 将结果拼接为"{6@LOGIN_PASSWORD_WRONG}\r\n"
            } else if(result == LOGIN_ERROR_LIGINED) { // 如果登录错误已经在线
                strcat(resu, "{6@DON`t LOGIN AGAIN}\r\n"); // 将结果拼接为"{6@DON`t LOGIN AGAIN}\r\n"
            }

            send(msg_socket, resu, strlen(resu), 0); // 将结果发送给客户端

        } else if(msg[1] == 48 + SENDMSG_FLAG){ // 如果消息为发送消息
            distributeMessage(msg_socket, msg); // 调用distributeMessage函数，将消息分发给其他客户端
        }
    }
}



int accountProcess(char* msg, int msg_socket) { // 定义一个函数，用于处理账户相关的请求，参数为消息内容和客户端套接字
    char username[10]; // 定义一个变量，用于存储用户名

    char password[16]; // 定义一个变量，用于存储密码
    int account_length = 0; // 定义一个变量，用于记录用户名或密码的长度
    int is_username = 1; // 定义一个变量，用于标记当前处理的是用户名还是密码
    for(int i = 0; i < strlen(msg) - 3; i++) { // 遍历消息内容，除去最后三个字符
        if(is_username) { // 如果当前处理的是用户名

            if(msg[i] == '@') { // 如果遇到@符号，表示用户名结束
                is_username = 0; // 将标记改为处理密码
                account_length = 0; // 将长度重置为0
                continue; // 跳过当前字符
            }

            username[account_length++] = msg[i]; // 将当前字符添加到用户名中

        } else { // 如果当前处理的是密码
            password[account_length++] = msg[i]; // 将当前字符添加到密码中
        }
    }

    int login_result = accountLogin(username, password, msg_socket); // 调用accountLogin函数，尝试登录，并返回结果

    if(login_result != 0) { // 如果登录结果不为0，表示登录成功或失败
        return login_result; // 返回登录结果
    }

    accountCreate(username, password); // 调用accountCreate函数，创建新用户

    return LOGIN_SUCCESS_NEW_USER; // 返回登录成功并创建新用户的状态
}

int accountLogin(char* username, char* password, int msg_socket) { // 定义一个函数，用于登录账户，参数为用户名，密码和客户端套接字
    for(int i = 0; i <= sum_user; i++) { // 遍历所有用户

        if(!strcmp(users[i].username, username)) { // 如果找到匹配的用户名
            if(!strcmp(users[i].password, password)) { // 如果密码也匹配
                // login success

                for(int relogin_test_count = 0; relogin_test_count <= sum_online_user; relogin_test_count++) { // 遍历所有在线用户

                    if(online_users[relogin_test_count].sockfd == msg_socket) { // 如果发现已经登录过
                        return LOGIN_ERROR_LIGINED; // 返回登录错误已经在线的状态
                    }
                }

                sum_online_user++; // 将在线用户数加一
                strcpy(online_users[sum_online_user].nikename, users[i].nikename); // 将用户的昵称复制到在线用户数组中
                online_users[sum_online_user].status = 1; // 将用户的状态设置为在线
                online_users[sum_online_user].sockfd = msg_socket; // 将用户的套接字设置为客户端套接字
                return LOGIN_SUCCESS; // 返回登录成功的状态
            } else { // 如果密码不匹配
                return LOGIN_PASSWORD_WRONG; // 返回登录密码错误的状态
            }
        }
    }

    return 0; // 返回0表示没有找到匹配的用户名
}

void accountCreate(char* username, char* password) { // 定义一个函数，用于创建账户，参数为用户名和密码
    sum_user++; // 将用户数加一

    struct user new_user; // 定义一个结构体变量，用于存储新用户的信息
    memset(&new_user, 0, sizeof(new_user)); // 将新用户的信息初始化为0
    strcpy(new_user.username, username); // 将用户名复制到新用户的信息中
    strcpy(new_user.password, password); // 将密码复制到新用户的信息中

    char nikename[10]; // 定义一个变量，用于存储新用户的昵称
    memset(nikename, '\0', 10); // 将昵称初始化为0
    strcat(nikename, "HJKSDK"); // 将"HJKSDK"添加到昵称中
    nikename[strlen(nikename)] = 48 + sum_user; // 将用户数转换为字符添加到昵称中
    strcpy(new_user.nikename, nikename); // 将昵称复制到新用户的信息中

    users[sum_user] = new_user; // 将新用户的信息添加到用户数组中
}

void distributeMessage(int sock_client, char* msg) { // 定义一个函数，用于分发消息，参数为客户端套接字和消息内容
    char* recvMsg = "{6@ok}\r\n"; // 定义一个变量，用于存储返回给客户端的消息

    for(int i = 0; i <= sum_online_user; i++) { // 遍历所有在线用户
        struct online_user the_user = online_users[i]; // 获取当前在线用户的信息
        if(the_user.sockfd != sock_client && the_user.status == 1) { // 如果当前在线用户不是发送消息的客户端，并且状态为在线
            char ret_msg[1024]; // 定义一个变量，用于存储转发给当前在线用户的消息
            memset(ret_msg, '\0', 1024); // 将消息初始化为0
            strcat(ret_msg, "from "); // 将"from "添加到消息中
            strcat(ret_msg, the_user.nikename); // 将发送消息的客户端的昵称添加到消息中
            strcat(ret_msg, " :"); // 将" :"添加到消息中
            strcat(ret_msg, msg); // 将消息内容添加到消息中

            send(the_user.sockfd, ret_msg, strlen(ret_msg), 0); // 将消息发送给当前在线用户
        }
    }

    send(sock_client, recvMsg, strlen(recvMsg), 0); // 将"{6@ok}\r\n"发送给发送消息的客户端
}

int main(){
    char* argc[3]={" ","172.18.0.2","12345"};
    server(3,argc);
    return 0;
}
