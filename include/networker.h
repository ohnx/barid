#ifndef __NETWORKER_H_INC
#define __NETWORKER_H_INC

/* struct barid_conf */
#include "common.h"

int networker_loop(void *z);

/* verbs that this server supports */
enum known_verbs {
    V_DATA,
    V_EHLO,
    V_HELO,
    V_MAIL,
    V_NOOP,
    V_QUIT,
    V_RSET,
    V_RCPT,
    V_STLS, /* starttls */
    V_UNKN /* unknown */
};

/* handle for networkers */
struct networker {
    /* thread handle and stack */
    int pid;
    void *stack;

    /* main barid config */
    struct barid_conf *sconf;

    /* epoll fd */
    int efd;

    /* serialization fd */
    int pfd;
};

#endif /* __NETWORKER_H_INC */
