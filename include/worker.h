#ifndef __WORKER_H_INC
#define __WORKER_H_INC

/* pthread_t */
#include <pthread.h>

void *worker_loop(void *z);

/* handle for workers */
struct worker {
    /* thread handle */
    pthread_t thread;

    /* epoll fd */
    int efd;

    /* serialization fd */
    int pfd;
};

enum state {
    BRANDNEW,
    HELO,
    MAIL,
    RCPT,
    DATA,
    END_DATA,
    QUIT
};

/* handle for clients */
struct client {
    int cfd;
    enum state state;
};

#endif /* __WORKER_H_INC */
