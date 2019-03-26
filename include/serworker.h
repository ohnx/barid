#ifndef __SERWORKER_H_INC
#define __SERWORKER_H_INC

/* pthread_t */
#include <pthread.h>

int serworker_deliver(int fd, struct mail *mail);
void *serworker_loop(void *z);

/* handle for SERWORKERs */
struct serworker {
    /* thread handle */
    pthread_t thread;

    /* serialization fd */
    int pfd;
};

#endif /* __SERWORKER_H_INC */
