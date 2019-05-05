/* this file does most of disk i/o */
/* free() */
#include <stdlib.h>
/* write(), read() */
#include <unistd.h>

/* running flag */
#include "common.h"
/* struct serworker */
#include "serworker.h"
/* logger_log() */
#include "logger.h"

int serworker_deliver(int fd, struct mail *mail) {
    return sizeof(mail) - write(fd, &mail, sizeof(mail));
}

void *serworker_loop(void *z) {
    struct serworker *self = (struct serworker *)z;
    struct mail *mail;
    unsigned int len;

start:
    /* read in the pointer to the mail object to serialize */
    len = read(self->pfd, &mail, sizeof(mail));

    if (len != sizeof(mail)) {
        /*logger_log(WARN, "Failed to deliver a mail!");*/
        goto next;
    }

    /* serialize it */
    logger_log(INFO, "Delivering mail at address %p", mail);

next:
    if (!running) goto end;
    else goto start;

end:
    return NULL;
}
