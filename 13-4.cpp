//
// Created by beida on 2023/5/19.
//

/*
    使用多进程和共享内存的聊天室服务端
    某一个用户发出消息，其余人连接到服务器上的用户皆能收到该消息
*/
#define _GNU_SOURCE 1

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT 65535
#define PROCESS_LIMIT 65536
#define MAX_EVENT_NUMBER 1024

struct client_data {
    sockaddr_in address;   // 客户端socket地址
    int connfd; // socket文件描述符
    pid_t pid;  // 处理这个连接的子进程pid
    int pipefd[2];  // 和父进程通信用的管道
};

static const char *shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd; // 共享内存标识符，通过shmget获得
char *share_mem = 0;
// 客户连接数组，进程用客户连接的编号来索引数组，即可取得相关客户连接数据
// key--客户连接编号，value--对应客户的client_data
client_data *users = 0;
// key--进程PID， value--该进程处理的客户连接的编号
int *sub_process = 0;
// 当前客户数量
int user_count = 0;
bool stop_child = false;

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//这是一个用于向 epoll 实例中添加文件描述符的函数。
void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET; // 注册可读事件
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理函数
void sig_handler(int sig) {
    // 保留原来的errno，在函数最后恢复，以保证函数的可重入性
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char *) &msg, 1, 0); // 将信号值写入管道，以通知主循环
    errno = save_errno;
}

/*
这是一个用于处理信号的函数。当该进程接收到一个指定信号时，操作系统会调用这个函数，
 并将该信号的信号值传递给该函数的 sig 参数。

函数主要的功能是将接收到的信号通过管道的方式发送给主循环，以通知主循环有信号到达。这里使用了信号管道技术，
 通过管道将信号值传递给主循环，可以避免信号处理中断主循环导致竞争、死锁等问题。

函数内部首先保存了当前的 errno 值，以保证函数的可重入性（也就是说，可以在多个线程或多个信号处理函数之间安全调用这个函数）。
然后将接收到的信号值写入了信号管道，以通知主循环。最后，恢复到原来的 errno 值，
 以便后续的代码能够正常地处理可能发生的错误。

需要注意的是，send() 函数内部可能会修改 errno 值，因此需要在处理完 send() 函数之后将 errno 值恢复为原来的值，
 以保证函数的可重入性。
*/

//这是一个用于注册信号处理函数的函数。
void addsig(int sig, void (*handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) //是否需要在系统调用被信号中断后自动重启该系统调用。
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask); // 设置所有信号
    // 为信号注册处理函数
    assert(sigaction(sig, &sa, NULL) != -1);    //使用 sigaction() 函数将信号处理函数注册到操作系统中。
}

void del_resource() {
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name); // 删除共享内存
    delete[]users;
    delete[]sub_process;
}

// 停止一个子进程
void child_term_handler(int sig) {
    stop_child = true;
}

// 运行子进程
// idx 该子进程处理的客户连接编号
// users 保存所有客户连接数据的数组
// share_mem 表示共享内存的起始地址
int run_child(int idx, client_data *users, char *share_mem) {   //子进程
    epoll_event events[MAX_EVENT_NUMBER];
    // 子进程用epoll监听两个文件描述符:
    // 客户连接socket 、与父进程通信的管道文件描述符
    int child_epollfd = epoll_create(5);
    assert(child_epollfd != -1);
    int connfd = users[idx].connfd;
    addfd(child_epollfd, connfd);
    int pipefd = users[idx].pipefd[1]; // 获取管道写端
    int ret;
    // 子进程设置自己的终止信号处理函数
    addsig(SIGTERM, child_term_handler, false);

    while (!stop_child) {
        int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) { //检查某个系统调用是否被信号中断
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            // 客户连接有数据来
            if ((sockfd == connfd) && (events[i].events & EPOLLIN)) {   //当监听套接字有读请求
                memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);
                // 将客户数据读取到对应的读缓存中
                // 该缓存时共享内存的一段，始于 idx * BUFFER_SIZE处
                // 长度为BUFFER_SIZE。各个连接的读缓存时共享的
                ret = recv(connfd, share_mem + idx * BUFFER_SIZE, BUFFER_SIZE - 1, 0);
                if (ret < 0) // recv失败
                {
                    if (errno != EAGAIN) {  //非阻塞被阻塞
                        stop_child = true;
                    }
                } else if (ret == 0) // 连接断开
                {
                    stop_child = true;
                } else {
                    // 成功读取客户数据后就通知主进程（通过管道）来处理
                    // 发送出该子进程处理的【客户连接编号】给主进程
                    send(pipefd, (char *) &idx, sizeof(idx), 0);
                }
            }
                // 主进程通知本进程（通过管道）将第 client个客户的数据发送到本进程负责的客户端
            else if ((sockfd == pipefd) && (events[i].events & EPOLLIN)) {  //来自主进程的读请求
                int client = 0;
                // 接受主进程发来的数据，即有客户数据到达的连接的编号
                ret = recv(sockfd, (char *) &client, sizeof(client), 0);
                if (ret < 0) {
                    if (errno != EAGAIN) {
                        stop_child = true;
                    }
                } else if (ret == 0) {
                    stop_child = true;
                } else {
                    send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
                }
            }
            else {
                continue;
            }
        }
    }
    close(connfd);
    close(pipefd);
    close(child_epollfd);
    return 0;
}

// 父进程
int main(int argc, char *argv[]) {

//    if (argc <= 2) {
//        printf("usage: %s ip_address port_num\n", basename(argv[0]));
//        return 1;
//    }
    const char *ip = "172.18.0.2";
    int port = atoi("1236");

    int ret = 0;

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    addr.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    // 设置端口复用
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ret = bind(listenfd, (struct sockaddr *) &addr, sizeof(addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);
//----------------------------------------------------------
    user_count = 0;
    users = new client_data[USER_LIMIT + 1];
    sub_process = new int[PROCESS_LIMIT];
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        sub_process[i] = -1; // init
    }

    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd); // 将listenfd上epoll树进行监视读事件

    // 有血缘关系的进程之间通信的管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]); // 写端设置非阻塞
    addfd(epollfd, sig_pipefd[0]); // 监视读端写事件

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, sig_handler);
    bool stop_server = false;
    bool terminate = false;

    // 创建共享内存，作为所有客户socket连接的读缓存！！！
    // 权限都设置为可读可写
    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    assert(shmfd != -1);
    // ftruncate()会将参数shmfd指定的文件大小改为USER_LIMIT * BUFFER_SIZE指定的大小。
    ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);
    assert(ret != -1);

    // 内存映射 ,把shmfd指向的内存从0开始映射到内存，最大映射长度USER_LIMIT * BUFFER_SIZE
    // MAP_SHARED：指定该共享内存可以被多个进程共享。
    share_mem = (char *) mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    close(shmfd);
    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            // 新客户连接到来
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *) &client_address, &client_addrlength); // 建立通信的fd

                if (connfd < 0) {
                    printf("errno is : %d\n", errno);
                    continue;
                }
                if (user_count >= USER_LIMIT) {
                    const char *info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, sizeof(int), 0);
                    close(connfd);
                    continue;
                }

                // 保存第 user_count个客户连接相关的数据
                users[user_count].address = client_address;
                users[user_count].connfd = connfd;
                // 在主进程和子进程间建立管道，以传递必要的数据
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);

                pid_t pid = fork(); // 每个连接单独开一个子进程为其处理IO逻辑
                if (pid < 0) {
                    close(connfd);
                    continue;
                }
                else if (pid == 0) // child
                {
                    // 子进程会把父进程的变量都拷贝一份
                    // 所以要关闭掉不用的文件描述符
                    close(epollfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    run_child(user_count, users, share_mem);
                    munmap((void *) share_mem, USER_LIMIT * BUFFER_SIZE);
                    exit(0);
                }
                else // parent
                {
                    close(connfd);
                    close(users[user_count].pipefd[1]); // 父进程管道写端关闭
                    addfd(epollfd, users[user_count].pipefd[0]); // FLAG1
                    users[user_count].pid = pid;

                    // 建立 进程pid 和 客户连接在数组users中的索引 的映射
                    sub_process[pid] = user_count;
                    user_count++;
                }
            }

            // 处理信号事件
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)) {   //信号读端有信号
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) {    //如果返回值为负数，则表示出现了错误
                    continue; // 跳到下一个事件处理
                }
                else if (ret == 0) {    //则表示管道已经被关闭 写端被关闭
                    continue;
                }
                else {
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {

                            // 子进程退出，表示有某个客户端关闭了连接
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                    // 用子进程pid取得被关闭的客户连接编号（即在users数组中的index）

                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1; // 清空映射
                                    if ((del_user < 0) || (del_user > USER_LIMIT)) {
                                        continue;
                                    }
                                    // 清除第del_user个客户连接使用的相关数据, 对应FLAG1位置epoll_ctl_add
                                    // 解除监视对应客户端的管道读端监视
                                    epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0);
                                    close(users[del_user].pipefd[0]);
                                    users[del_user] = users[--user_count]; // 把最后一个数据填充到删除的位置上
                                    sub_process[users[del_user].pid] == del_user;
                                }
                                if (terminate && user_count == 0) //
                                {
                                    stop_server = true;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT: {
                                // 结束服务器程序
                                printf("kill all the child now\n");
                                if (user_count == 0) {
                                    stop_server = true;
                                    break;
                                }
                                for (int i = 0; i < user_count; ++i) {
                                    int pid = users[i].pid;
                                    kill(pid, SIGTERM); // 强制终止进程pid
                                }
                                terminate = true;
                                break;
                            }
                            default: {
                                break;
                            }
                        }
                    }
                }
            }
                // 某个子进程向父进程写入了数据
            else if (events[i].events & EPOLLIN) {
                int child = 0;
                // 读取管道数据，child记录是哪个客户连接有数据到达
                ret = recv(sockfd, (char *) &child, sizeof(child), 0);
                printf("read data from child across pipe\n");
                if (ret == -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    // 向负责处理第child客户连接的子进程之外的其他子进程发送消息
                    // 通知他们有客户数据要写
                    for (int j = 0; j < user_count; ++j) {
                        // 管道两端可读克写
                        if (users[j].pipefd[0] != sockfd) {
                            printf("send data to child across pipe\n");
                            send(users[j].pipefd[0], (char *) &child, sizeof(child), 0);
                        }
                    }
                }
            }
        }
    }
    del_resource();
    return 0;
}
