/* this file is in charge of abstracting network i/o in case of SSL */
/* calloc() */
#include <stdlib.h>
/* write(), read() */
#include <unistd.h>
/* all the stuff for mbed TLS */
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"
/* struct client, global vars */
#include "common.h"
/* function defs and stuff */
#include "net.h"
/* logger */
#include "logger.h"

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_config ssl_conf;
mbedtls_x509_crt srvcert;
mbedtls_pk_context pkey;

/* called in a thread context */
int net_rx(struct client *client) {
    if (!(sconf.flgs & SSL_ENABLED) || !(client->ssl))
        return read(client->cfd, client->buf + client->bio, LARGEBUF - client->bio - 1);
    return mbedtls_ssl_read(client->ssl, (unsigned char *)(client->buf + client->bio), LARGEBUF - client->bio - 1);
}

/* called in a thread context */
int net_tx(struct client *client, unsigned char *buf, size_t bufsz) {
    if (!(sconf.flgs & SSL_ENABLED) || !(client->ssl))
        return write(client->cfd, buf, bufsz);
    return mbedtls_ssl_write(client->ssl, (unsigned char *)buf, bufsz);
}

/* called in a thread context */
/* initiate SSL handshake */
int net_sssl(struct client *client) {
    int ret;
    if (!(sconf.flgs & SSL_ENABLED) || !client) return -1;

    if (client->ssl) {
        if (client->state == S_SSL_HS) goto hs_step;
        else return -2; /* SSL already started */
    }

    /* allocate memory for this context */
    client->ssl = calloc(sizeof(mbedtls_ssl_context), 1);
    if (!client->ssl) return -3;

    /* initialize ssl context */
    mbedtls_ssl_init(client->ssl);

    /* get context ready */
    mbedtls_ssl_setup(client->ssl, &ssl_conf);
    mbedtls_ssl_set_bio(client->ssl, &(client->cfd), mbedtls_net_send, mbedtls_net_recv, NULL);

    /* set the correct state */
    client->state = S_SSL_HS;

hs_step:
    /* perform SSL handshake */
    ret = mbedtls_ssl_handshake(client->ssl);

    switch (ret) {
    case 0:
        /* handshake done */
        client->state = S_HELO;
        break;
    case MBEDTLS_ERR_SSL_WANT_READ:
    case MBEDTLS_ERR_SSL_WANT_WRITE:
    case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS:
    case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS:
        /* handshake not done but will need more time */
        break;
    default:
        /* some sort of error here */
        return -4;
    }

    return 0;
}

/* called in a thread context */
int net_close(struct client *client) {
    if (!(sconf.flgs & SSL_ENABLED) || !(client->ssl))
        return close(client->cfd);
    mbedtls_ssl_close_notify(client->ssl);
    mbedtls_ssl_free(client->ssl);
    return 0;
}

int net_init(const char *ssl_cert, const char *ssl_key) {
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
        logger_log(ERR, "Failed to parse a certificate!"); return r;
    }

    /* parse the private key */
    r = mbedtls_pk_parse_keyfile(&pkey, ssl_key, NULL);
    if (r) {
        logger_log(ERR, "Failed to parse private key!"); return r;
    }

    /* seed the RNG */
    r = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)MAILVER, sizeof(MAILVER));
    if (r) {
        logger_log(ERR, "Failed to seed random number generator!"); return r;
    }

    /* set up SSL configuration */
    r = mbedtls_ssl_config_defaults(&ssl_conf,
                    MBEDTLS_SSL_IS_SERVER,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    MBEDTLS_SSL_PRESET_DEFAULT);
    if (r) {
        logger_log(ERR, "Failed to configure SSL!"); return r;
    }

    /* set up rng to use the entropy source ctr_drbg */
    mbedtls_ssl_conf_rng(&ssl_conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    /* configure own certificate */
    r = mbedtls_ssl_conf_own_cert(&ssl_conf, &srvcert, &pkey);
    if (r) {
        logger_log(ERR, "Failed to set own certificate!"); return r;
    }

    return 0;
}

void net_deinit() {
    mbedtls_x509_crt_free(&srvcert);
    mbedtls_pk_free(&pkey);
    mbedtls_ssl_config_free(&ssl_conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}