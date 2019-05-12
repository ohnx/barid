#ifndef __SERWORKER_H_INC
#define __SERWORKER_H_INC

int serworker_deliver(int fd, struct mail *mail);
#ifndef USE_PTHREADS
int serworker_loop(void *z);
#else
#include <pthread.h>
void *serworker_loop(void *z);
#endif

/* handle for serworkers */
struct serworker {
#ifndef USE_PTHREADS
    /* thread handle and stack */
    int pid;
    void *stack;
#else
    /* thread handle */
    pthread_t thread;
#endif

    /* serialization fd */
    int pfd;
};

#endif /* __SERWORKER_H_INC */
