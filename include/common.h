#ifndef __COMMON_H_INC
#define __COMMON_H_INC

/* FILE */
#include <stdio.h>
/* sig_atomic_t */
#include <signal.h>
/* mbedtls_ssl_context */
#include "mbedtls/ssl.h"

/* server configuration*/
struct barid_conf {
    FILE *logger_fd;
    unsigned char flgs;
};

#define SSL_ENABLED 0x1

/* states */
enum state {
    /* brand new; waiting for write ready to send greeting */
    BRANDNEW,
    /* waiting for HELO */
    HELO,
    /* waiting for MAIL */
    MAIL,
    /* waiting for RCPT */
    RCPT,
    /* waiting for RCPT or DATA*/
    DATA,
    /* waiting for end of DATA ('.') */
    END_DATA,
    /* waiting for SSL handshake to complete */
    SSL_HS
};

/* handle for clients */
struct client {
    int cfd;
    enum state state;
    mbedtls_ssl_context *ssl;
    unsigned int bio;
    unsigned char buf[LARGEBUF];
};

/* this struct holds internal info */
struct mail_internal_info {
    int to_total_len;
    int data_total_len;
    unsigned char using_ssl;
    struct sockaddr_storage *origin_ip;
};

/* mail info that networker fills out and that serworker serializes */
struct mail {
    /* the server that this mail is from */
    int froms_c;
    char *froms_v;
    /* the email address that this mail is from */
    int from_c;
    char *from_v;
    /* email addresses this email is to */
    int to_c; /* total length of email address string */
    char *to_v; /* null-separated array of to email addresses */
    /* message data */
    int data_c;
    char *data_v;
    /* extra information (a null `extra` indicates this email is READONLY) */
    struct mail_internal_info *extra;
};


/* max length of an email's recipients (in bytes)
 * 512 recipients @ 256 bytes per email = 131072 B (128 KiB) */
#define MAIL_MAX_TO_C           131072

/* max length of an email (in bytes) - default is 16777216 B (16 MiB) */
#define MAIL_MAX_DATA_C         16777216

/* version string */
#define MAILVER "barid v1.0.0a"

/* buffer for a single line of input from a client */
#define LARGEBUF                4096

/* server configuration */
extern struct barid_conf sconf;

/* running flag */
extern sig_atomic_t running;

#endif /* __COMMON_H_INC */
