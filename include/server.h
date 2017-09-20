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

extern char *server_greeting;
extern int server_greeting_len;

void server_initsocket(int *fd);
int server_bindport(int fd, int port);

#endif /* __SERVER_H_INC */
