#define _GNU_SOURCE

#include <stdio.h>
#include <sys/socket.h>
#include <dlfcn.h>

ssize_t send(int socket, const void *buffer, size_t length, int flags) {
    ssize_t (*orig_send)(int, const void *, size_t, int);
    orig_send = dlsym(RTLD_NEXT, "send");

    /* print sending data */
    printf("%02d << ```%s```\n", socket, (const char *)buffer);

    return (*orig_send)(socket, buffer, length, flags);
}

ssize_t recv(int socket, void *buffer, size_t length, int flags) {
    ssize_t (*orig_recv)(int, void *, size_t, int);
    ssize_t ret;
    orig_recv = dlsym(RTLD_NEXT, "recv");

    ret = (*orig_recv)(socket, buffer, length, flags);

    /* print received data */
    printf("%02d >> ```%s```\n", socket, (const char *)buffer);

    return ret;
}
