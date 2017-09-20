#ifndef __MAIL_H_INC
#define __MAIL_H_INC

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAIL_ERROR_NONE         0
#define MAIL_ERROR_PARSE        1
#define MAIL_ERROR_INVALIDEMAIL 2
#define MAIL_ERROR_RCPTMAX      3
#define MAIL_ERROR_DATAMAX      3
#define MAIL_ERROR_PROGRAM      8
#define MAIL_ERROR_OOM          9

/* this struct holds internal info */
struct mail_internal_info {
    int to_total_len;
    int data_total_len;
    struct sockaddr_storage *origin_ip;
};

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

/* what attribute to write to */
enum mail_attr {
    FROMS,
    FROM,
    TO
};

/* serialize output format */
enum mail_sf {
    STDOUT = 1,
    BINARY = 3,
    MAILBOX = 4
};

struct mail *mail_new_internal(int hasExtra);
#define mail_new()  mail_new_internal(1)
int mail_setattr(struct mail *email, enum mail_attr attr, const char *data);
int mail_addattr(struct mail *email, enum mail_attr attr, const char *data);
int mail_appenddata(struct mail *email, const char *data);
void mail_destroy(struct mail *email);
void mail_serialize(struct mail *email, enum mail_sf format, int sock);

#endif /* __MAIL_H_INC */
