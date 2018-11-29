#ifndef __SERVER_H_INC
#define __SERVER_H_INC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "common.h"
#include "mail.h"
#include "smtp.h"
#include "ssl.h"

extern char *server_greeting;
extern int server_greeting_len;
extern char *server_hostname;
extern int enable_ssl;

void server_initsocket(int *fd);
int server_bindport(int fd, int port);

#endif /* __SERVER_H_INC */
