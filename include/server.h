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

#include "common.h"
#include "mail.h"
#include "smtp.h"

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
