#include "../../include/wrap.h"

// #include <stdio.h>
// #include <string.h>

ssize_t wrap_recv(int sockfd, void *buf, size_t len, int flags)
{
    int readn = 0, tmp;
    while(1) {
        tmp = recv(sockfd, (buf + readn), (len - readn), flags);
        if (tmp == -1 && errno == EAGAIN) continue;
        if (tmp == -1) return -1;
        readn += tmp;
        if (readn == len) return readn;
    }
    return -1;
}

ssize_t wrap_send(int sockfd, const void *buf, size_t len, int flags)
{
    int writen = 0, tmp;
    while (1) { 
        tmp = send(sockfd, (buf + writen), (len - writen), flags);
        if (tmp == -1 && errno == EAGAIN) continue;
        if (tmp == -1) return -1;
        writen += tmp;
        if (writen == len) return writen;
    }
    return -1; 
}