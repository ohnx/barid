/* this file does most of disk i/o */
/* free() */
#include <stdlib.h>
/* epoll-family functions */
#include <sys/epoll.h>
/* write(), read() */
#include <unistd.h>

/* running flag */
#include "common.h"
/* struct networker */
#include "networker.h"
/* logger_log() */
#include "logger.h"

void *serworker_loop(void *z) {
    struct serworker *self = (struct serworker *)z;
    struct epoll_event epevnt;
    struct client *client;
    unsigned char buf[4096];
    unsigned int len;

start:
    if (epoll_wait(self->efd, &epevnt, 1, -1) < 0) goto end;
    client = (struct client *)(epevnt.data.ptr);

    /* error occurred */
    if ((epevnt.events & EPOLLRDHUP) ||
        (epevnt.events & EPOLLERR) ||
        (epevnt.events & EPOLLHUP)) {
        logger_log(INFO, "Client %d disconnected!", client->cfd);
        /* clean up client data */
        free(epevnt.data.ptr);
        goto start;
    }

    if ((epevnt.events & EPOLLOUT) && client->state == BRANDNEW) {
        /* brand new connection */
        logger_log(INFO, "Client %d connected!", client->cfd);
        write(client->cfd, "hello!\n", 7);

        /* update stage */
        client->state = HELO;

        goto next;
    }

    if (!(epevnt.events & EPOLLIN)) {
        logger_log(WARN, "wat %d", epevnt.events);
        goto next;
    }

    switch(client->state) {
    default:
        len = read(client->cfd, buf, 4096);
        write(client->cfd, buf, len);
        break;
    }

next:
    /* resume listening for data from the socket */
    epevnt.events = EPOLLIN | EPOLLONESHOT;
    if (epoll_ctl(self->efd, EPOLL_CTL_MOD, client->cfd, &epevnt) < 0) {
        logger_log(WARN, "Failed to listen to client %d!", client->cfd);
        free(epevnt.data.ptr);
        close(client->cfd);
        goto start;
    }

    if (!running) goto end;
    else goto start;

end:
    return NULL;
}
