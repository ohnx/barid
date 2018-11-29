/* this file deals with sending and receiving information */
#include "ssl.h"

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_config ssl_conf;
mbedtls_x509_crt srvcert;
mbedtls_pk_context pkey;

/* called in a thread context */
int ssl_conn_rx(struct connection *conn, char *buf, size_t bufsz) {
    if (!conn->ssl || !(conn->ssl->flags&0x1))
        return recv(conn->fd, buf, bufsz, 0);
    return mbedtls_ssl_read(&(conn->ssl->ssl), (unsigned char *)buf, bufsz);
}

/* called in a thread context */
int ssl_conn_tx(struct connection *conn, char *buf, size_t bufsz) {
    if (!conn->ssl || !(conn->ssl->flags&0x1))
        return send(conn->fd, buf, bufsz, 0);
    return mbedtls_ssl_write(&(conn->ssl->ssl), (unsigned char *)buf, bufsz);
}

/* called in a thread context */
/* initiate SSL handshake */
int ssl_conn_start(struct connection *conn) {
    int ret;
    struct ssl_ctx *ctx = (struct ssl_ctx *)conn->ssl;
    if (!ctx) return -1;
    /* SSL already started */
    if (ctx->flags&0x1) return -2;

    /* initialize ssl context */
    mbedtls_ssl_init(&(ctx->ssl));

    /* get context ready */
    mbedtls_ssl_setup(&(ctx->ssl), &ssl_conf);
    mbedtls_ssl_set_bio(&(ctx->ssl), &(conn->fd), mbedtls_net_send, mbedtls_net_recv, NULL);

    /* perform SSL handshake */
    while ((ret = mbedtls_ssl_handshake(&(ctx->ssl))) != 0)
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            return -1;

    /* ssl enabled flag */
    ctx->flags |= 0x1;
    return 0;
}

/* called in a thread context */
int ssl_conn_close(struct connection *conn) {
    if (!conn->ssl || !(conn->ssl->flags&0x1))
        return close(conn->fd);
    mbedtls_ssl_close_notify(&(conn->ssl->ssl));
    mbedtls_ssl_free(&(conn->ssl->ssl));
    return 0;
}

int ssl_global_init(const char *ssl_cert, const char *ssl_key) {
    int r;

    /* initialize all the variables */
    mbedtls_x509_crt_init(&srvcert);
    mbedtls_pk_init(&pkey);
    mbedtls_ssl_config_init(&ssl_conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    /* parse the certificates */
    r = mbedtls_x509_crt_parse_file(&srvcert, ssl_cert);
    if (r) {
        fprintf(stderr, ERR"Failed to parse a certificate!\n"); return r;
    }

    /* parse the private key */
    r = mbedtls_pk_parse_keyfile(&pkey, ssl_key, NULL);
    if (r) {
        fprintf(stderr, ERR"Failed to parse private key!\n"); return r;
    }

    /* seed the RNG */
    r = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)MAILVER, strlen(MAILVER));
    if (r) {
        fprintf(stderr, ERR"Failed to seed random number generator!\n"); return r;
    }

    /* set up SSL configuration */
    r = mbedtls_ssl_config_defaults(&ssl_conf,
                    MBEDTLS_SSL_IS_SERVER,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    MBEDTLS_SSL_PRESET_DEFAULT);
    if (r) {
        fprintf(stderr, ERR"Failed to configure SSL!\n"); return r;
    }

    /* set up rng to use the entropy source ctr_drbg */
    mbedtls_ssl_conf_rng(&ssl_conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    /* configure own certificate */
    r = mbedtls_ssl_conf_own_cert(&ssl_conf, &srvcert, &pkey);
    if (r) {
        fprintf(stderr, ERR"Failed to set own certificate!\n"); return r;
    }

    return 0;
}

void ssl_global_deinit() {
    mbedtls_x509_crt_free(&srvcert);
    mbedtls_pk_free(&pkey);
    mbedtls_ssl_config_free(&ssl_conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}
