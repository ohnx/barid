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
