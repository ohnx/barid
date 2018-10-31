/* this file deals with sending and receiving information */
#include "ssl.h"

int ssl_conn_rx(struct connection *conn, char *buf, size_t bufsz) {
    if (!conn->ssl)
        return recv(conn->fd, buf, bufsz, 0);
    return 0;
}

int ssl_conn_tx(struct connection *conn, char *buf, size_t bufsz) {
    if (!conn->ssl)
        return send(conn->fd, buf, bufsz, 0);
    return 0;
}

int ssl_conn_start() {
    return -1;
}

int ssl_conn_close() {
    return -1;
}

int ssl_init() {
    return 0;
}
