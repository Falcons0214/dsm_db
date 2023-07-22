#ifndef WRAP_H
#define WRAP_H

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

ssize_t wrap_recv(int, void*, size_t, int);
ssize_t wrap_send(int, const void*, size_t, int);

#endif /* WRAP_H */