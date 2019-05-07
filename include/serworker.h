#ifndef __SERWORKER_H_INC
#define __SERWORKER_H_INC

int serworker_deliver(int fd, struct mail *mail);
int serworker_loop(void *z);

/* handle for SERWORKERs */
struct serworker {
    /* thread handle and stack */
    int pid;
    void *stack;

    /* serialization fd */
    int pfd;
};

#endif /* __SERWORKER_H_INC */
