#ifndef __SSL_H_INC
#define __SSL_H_INC

#include <sys/socket.h>
#include <stdlib.h>

struct connection {
    int fd;
    unsigned char ssl;
};

int ssl_conn_rx(struct connection *conn, char *buf, size_t bufsz);
int ssl_conn_tx(struct connection *conn, char *buf, size_t bufsz);
int ssl_init();

#endif /* __SSL_H_INC */
