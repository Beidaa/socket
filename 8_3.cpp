#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>

#define BUFFER_SIZE 4096


enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER}; // 定义状态机的状态

enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN}; // 定义解析结果的状态

enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION}; // 定义 HTTP 请求状态

static const char* szret[] = {"I get a correct result\n", "Something wrong\n"}; // HTTP 请求处理结果提示信息

LINE_STATUS parse_line(char* buffer, int& checked_index, int& read_index) { // 解析 HTTP 请求中的每一行数据
    char temp;

    // 判断 buffer 中的每个字符，直到找到一个 \r 或 \n
    for(; checked_index < read_index; checked_index++) {
        temp = buffer[checked_index];
        if(temp == '\r') { // 如果当前字符为 \r
            if((checked_index + 1) == read_index) { // 判断下一个是否超出界限
                return LINE_OPEN; // 还需要继续接收数据
            } else if(buffer[checked_index + 1] == '\n') { // 如果下一个字符为 \n，则表示当前行数据已经接收完毕

                buffer[checked_index++] = '\0'; // 将当前字符改为 '\0'，作为字符串结束符
                buffer[checked_index++] = '\0'; // 将下一个字符也改为 '\0'，以便统计接收到的数据长度
                return LINE_OK; // 当前行数据解析完成
            }

            return LINE_BAD; // 数据格式错误，解析失败
        } else if(temp == '\n') { // 如果当前字符为 \n

            if((checked_index > 1) && buffer[checked_index - 1] == '\r') { // 判断前一个字符是否为 \r

                buffer[checked_index++] = '\0'; // 将当前字符改为 '\0'
                buffer[checked_index++] = '\0'; // 将下一个字符也改为 '\0'
                return LINE_OK; // 当前行数据解析完成
            }
            return LINE_BAD; // 数据格式错误，解析失败
        }
    }

    return LINE_OPEN; // 当前行数据未接收完毕，需要继续接收
}

HTTP_CODE parse_requestline(char* temp, CHECK_STATE& checkstate) { // 解析 HTTP 请求行
    // 查找字符串中的空格或制表符，表示请求方法与请求 URL 的分隔位置
    char* url = strpbrk(temp, " \t");
    if(!url) {
        return BAD_REQUEST; // 没有找到分隔符，请求格式错误
    }
    *url++ = '\0'; // 将空格或制表符改为字符串结束符，且将 url 指针移动到下一个位置

    char* method = temp; // 获取请求方法
    // 只支持 GET 方法
    if(strcasecmp(method, "GET") == 0) { // 比较字符串是否相等，不区分大小写
        printf("The request method is GET\n"); // 打印请求方法
    } else {
        return BAD_REQUEST; // 不支持的请求方法
    }

    url += strspn(url, " \t"); // 跳过 URL 前面的空格和制表符
    char* version = strpbrk(url, " \t"); // 获取 HTTP 版本号
    if(!version) {
        return BAD_REQUEST; // 没有找到 HTTP 版本号，请求格式错误
    }

    *version++ = '\0'; // 将空格或制表符改为字符串结束符，且将 version 指针移动到下一个位置
    version += strspn(version, " \t"); // 跳过 HTTP 版本号后面的空格和制表符
    // 只支持 HTTP/1.1 版本
    if(strcasecmp(version, "HTTP/1.1") != 0) {
        return BAD_REQUEST; // 不支持的 HTTP 版本号，请求格式错误
    }
    // 检查 URL 是否合法
    if(strncasecmp(url, "http://", 7) == 0) { // 必须以 "http://" 开头
        url += 7; // 将 url 指针移动到 "http://" 之后的位置
        url = strchr(url, '/'); // 查找第一个 "/"
    }
    if(!url || url[0] != '/') { // URL 格式不正确
        return BAD_REQUEST;
    }
    printf("The request URL is: %s\n", url); // 打印请求 URL

    checkstate = CHECK_STATE_HEADER; // 进入解析请求头部状态
    return NO_REQUEST; // 解析成功，返回状态码
}



HTTP_CODE parse_headers(char* temp) {
    if(temp[0] == '\0') { // 判断头部是否为空
        return GET_REQUEST; // 若为空，返回 GET_REQUEST 状态码
    } else if (strncasecmp(temp, "Host:", 5) == 0) { // 判断头部是否为 Host 字段
        temp += 5; // 移动指针到 Host 字段值的开头
        temp += strspn(temp, "\ t"); // 跳过字段值中的空格和制表符
        printf("The request host is: %s\n", temp); // 输出请求主机名
    } else {
        printf("I can not handle this header\n"); // 无法处理该头部
    }
    return NO_REQUEST; // 解析成功，返回 NO_REQUEST 状态码
}


HTTP_CODE parse_content(char* buffer, int& checked_index, CHECK_STATE&
checkstate, int& read_index, int& start_line) {
    LINE_STATUS linestatus = LINE_OK;
    HTTP_CODE retcode = NO_REQUEST;

    while((linestatus = parse_line(buffer, checked_index, read_index)) == LINE_OK) {
        // 循环解析缓冲区 buffer 中的每一行数据
        char* temp = buffer + start_line;
        start_line = checked_index; // 更新已经解析过的数据

        switch(checkstate) { // 根据当前状态进行处理
            case CHECK_STATE_REQUESTLINE:
                retcode = parse_requestline(temp, checkstate); // 解析请求行
                if(retcode == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            case CHECK_STATE_HEADER:
                retcode = parse_headers(temp); // 解析头部
                if(retcode == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if (retcode == GET_REQUEST) { // 头部为空
                    return GET_REQUEST;
                }
                break;
            default:
                return INTERNAL_ERROR; // 解析出现错误
        }
    }
    if(linestatus == LINE_OPEN) { // 没有解析到完整的行数据
        return NO_REQUEST;
    } else {
        return BAD_REQUEST; // 解析出现错误
    }
}


int main(int argc, char* argv[]) {
    // 检查命令行参数个数是否为 3
    if(argc <= 2) {
        printf("Wrong number of parameters\n");
        return 1;
    }

    // 从命令行参数中获取 IP 地址和端口号，并存储到结构体中
    const char * ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    // 创建套接字并绑定到指定的地址和端口上
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));

    // 如果绑定失败则尝试在不同的端口重新绑定
    while (ret == -1) {
        address.sin_port = htons(++port);
        ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
        printf("the port is %d\n", port);
    }

    // 将套接字设置为监听状态
    ret = listen(listenfd, 5);
    assert(ret != -1);

    // 接受客户端连接请求，返回新的套接字描述符
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int fd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);

    if(fd < 0) {
        // 如果接受连接失败则打印错误信息
        printf("errno is: %d\n", errno);
    } else {
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE);
        int date_read = 0;
        int read_index = 0;
        int checked_index = 0;
        int start_line = 0;

        // 使用简单的状态机解析 HTTP 请求
        CHECK_STATE checkstatus = CHECK_STATE_REQUESTLINE;
        while(1) {
            date_read = recv(fd, buffer + read_index, BUFFER_SIZE - read_index, 0);
            if(date_read == -1) {
                // 如果读取数据失败则跳出循环
                printf("reading failed\n;");
                break;
            } else if(date_read == 0) {
                // 如果远程客户端关闭了连接，则跳出循环
                printf("remote client had closed the connection\n");
                break;
            }
            read_index += date_read;

            // 解析 HTTP 请求
            HTTP_CODE result = parse_content(buffer, checked_index, checkstatus, read_index, start_line);
            if(result == NO_REQUEST) {
                // 若无有效请求则继续读取数据
                continue;
            } else if(result == GET_REQUEST) {
                // 若为 GET 请求则向客户端返回指定响应内容并跳出循环
                send(fd, szret[0], strlen(szret[0]), 0);
                break;
            } else {
                // 若不是 GET 请求则向客户端返回另一种响应内容并跳出循环
                send(fd, szret[1], strlen(szret[1]), 0);
                break;
            }
        }

        // 关闭套接字描述符
        close(fd);
    }

    // 关闭监听套接字并释放资源
    close(listenfd);
    return 0;
}

