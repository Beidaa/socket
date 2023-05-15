//
// Created by beida on 2023/5/15.
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

int main()
{
    int fds[2];
    socketpair(PF_UNIX, SOCK_STREAM, 0, fds);
    int pid = fork();
    if (pid == 0)
    {
        close(fds[0]);
        char a[] = "123899";
        send(fds[1], a, strlen(a), 0);
    }
    else if (pid > 0)
    {
        close(fds[1]);
        char b[20] {};
        recv(fds[0], b, 20, 0);
        printf("%s", b);
    }
}