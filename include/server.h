#ifndef __SERVER_H_INC
#define __SERVER_H_INC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "fancystuff.h"
#include "mail.h"

/* version string */
#define MAILVER "SMTP mail"

/* this is the buffer for a single line; mail size can be found in mail.h */
#define LARGEBUF        4096
/* this is the small buffer for output */
#define SMALLBUF        256

struct server {
    int socket;
    char *domain;
    pthread_t thread;
};

struct session {
    struct server *parent;
    int fd;
    struct mail *data;
};

void server_initsocket(struct server *state);
int server_bindport(struct server *state, int port);

#endif /* __SERVER_H_INC */
