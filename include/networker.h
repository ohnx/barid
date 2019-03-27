#ifndef __NETWORKER_H_INC
#define __NETWORKER_H_INC

/* pthread_t */
#include <pthread.h>

void *networker_loop(void *z);

/* verbs that this server supports */
enum known_verbs {
    DATA,
    EHLO,
    HELO,
    MAIL,
    NOOP,
    QUIT,
    RSET,
    RCPT,
    STLS, /* starttls */
    UNKN /* unknown */
};

/* handle for networkers */
struct networker {
    /* thread handle */
    pthread_t thread;

    /* epoll fd */
    int efd;

    /* serialization fd */
    int pfd;
};

#endif /* __NETWORKER_H_INC */
