#ifndef __MAIL_H_INC
#define __MAIL_H_INC

#define MAIL_ERROR_NONE         0
#define MAIL_ERROR_PARSE        1
#define MAIL_ERROR_INVALIDEMAIL 2
#define MAIL_ERROR_RCPTMAX      3
#define MAIL_ERROR_DATAMAX      3
#define MAIL_ERROR_USRNOTLOCAL  4
#define MAIL_ERROR_PROGRAM      8
#define MAIL_ERROR_OOM          9
#define MAIL_ERROR_MISC         10

/* what attribute to write to */
enum mail_attr {
    FROMS,
    FROM,
    TO,
    SSL_USED,
    REMOTE_ADDR
};

/* serialize output format */
enum mail_sf {
    NONE = 0,
    STDOUT = 1,
    BINARY = 3,
    MAILBOX = 4,
    BOTH = 5
};

void mail_set_allowed(struct barid_conf *conf);
struct mail *mail_new(const char *froms);
int mail_reset(struct mail *email);
int mail_setattr(struct mail *email, enum mail_attr attr, const char *data);
int mail_addattr(struct mail *email, enum mail_attr attr, const char *data);
int mail_appenddata(struct mail *email, const char *data);
void mail_destroy(struct mail *email);

#endif /* __MAIL_H_INC */
