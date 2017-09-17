#ifndef __MAIL_H_INC
#define __MAIL_H_INC

#include "common.h"
#include <stdio.h>
#include <stdlib.h>

/* this struct holds  */
struct mail_internal_info {
    int to_total_len;
    int data_total_len;
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
    /* extra information */
    struct mail_internal_info *extra;
};

enum mail_attr {
    FROM_S,
    FROM,
    TO
};

struct mail *mail_new();
int mail_setattr(struct mail *email, enum mail_attr attr, const char *data);
int mail_addattr(struct mail *email, enum mail_attr attr, const char * data);
int mail_appenddata(struct mail *email, const char *data);
void mail_clear(struct mail *email);
void mail_serialize(struct mail *email);

#endif /* __MAIL_H_INC */
