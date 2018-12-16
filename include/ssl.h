#ifndef __SSL_H_INC
#define __SSL_H_INC

#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"

#include "common.h"

struct ssl_ctx {
    unsigned char flags;
    mbedtls_ssl_context ssl;
};

struct connection {
    int fd;
    struct ssl_ctx *ssl;
};

#ifndef __FUZZ
int ssl_conn_rx(struct connection *conn, char *buf, size_t bufsz);
int ssl_conn_tx(struct connection *conn, char *buf, size_t bufsz);
int ssl_conn_start(struct connection *conn);
int ssl_conn_close(struct connection *conn);
int ssl_global_init();
void ssl_global_deinit();

#else
int ssl_conn_rx(struct connection *conn, char *buf, size_t bufsz);
int ssl_conn_tx(struct connection *conn, char *buf, size_t bufsz);
#endif /* __FUZZ */

#endif /* __SSL_H_INC */
