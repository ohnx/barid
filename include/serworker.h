#ifndef __NETWORKER_H_INC
#define __NETWORKER_H_INC

/* pthread_t */
#include <pthread.h>

void *serworker_loop(void *z);

/* handle for networkers */
struct networker {
    /* thread handle */
    pthread_t thread;

    /* serialization fd */
    int pfd;
};

#endif /* __NETWORKER_H_INC */
