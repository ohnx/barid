#ifndef __MAIL_H_INC
#define __MAIL_H_INC

#include "server.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/socket.h>

#define MAIL_ERROR_NONE         0
#define MAIL_ERROR_PARSE        1
#define MAIL_ERROR_INVALIDEMAIL 2
#define MAIL_ERROR_RCPTMAX      3
#define MAIL_ERROR_DATAMAX      3
#define MAIL_ERROR_USRNOTLOCAL  4
#define MAIL_ERROR_PROGRAM      8
#define MAIL_ERROR_OOM          9

/* what attribute to write to */
enum mail_attr {
    FROMS,
    FROM,
    TO
};

/* serialize output format */
enum mail_sf {
    NONE = 0,
    STDOUT = 1,
    BINARY = 3,
    MAILBOX = 4,
    BOTH = 5
};

struct mail *mail_new_internal(int hasExtra);
#define mail_new()  mail_new_internal(1)
int mail_setattr(struct mail *email, enum mail_attr attr, const char *data);
int mail_addattr(struct mail *email, enum mail_attr attr, const char *data);
int mail_appenddata(struct mail *email, const char *data);
void mail_destroy(struct mail *email);

int mail_serialize(struct mail *email, enum mail_sf format);
int mail_serialize_stdout(struct mail *email);
int mail_serialize_file(struct mail *email);

#endif /* __MAIL_H_INC */
