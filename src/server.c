/* this file reads in the configuration and initializes sockets and stuff */
/* malloc() */
#include <stdlib.h>
/* socket(), setsockopt(), bind(), listen() */
#include <sys/socket.h>
/* struct sockaddr_in6 */
#include <netinet/in.h>
/* fnctl() */
#include <unistd.h>
/* fnctl() */
#include <fcntl.h>
/* sig_atomic_t */
#include <signal.h>
/* epoll-family functions */
#include <sys/epoll.h>
/* pthread-family functions */
#include <pthread.h>

/* MAIL_VER */
#include "common.h"
/* logger_log() */
#include "logger.h"
/* struct networker */
#include "networker.h"
/* struct serworker */
#include "serworker.h"

/* global variables */
struct barid_conf sconf;
sig_atomic_t running = 1;

/* main function */
int main(int argc, char **argv) {
    int sfd, efd, pfd[2], cfd, port, nwork, swork, i, flg;
    struct networker *networkers;
    struct serworker *serworkers;
    struct epoll_event epint = {0};
    struct sockaddr_in6 saddr = {0};

    /* temp debug since configuration isn't set up */
    sconf.logger_fd = stderr;
    sconf.flgs = 0;
    port = 8712;
    nwork = 1;
    swork = 1;

    /* hi, world! */
    logger_log(INFO, "%s starting!", MAILVER);

    /* start by creating a socket */
    if ((sfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        logger_log(ERR, "Could not create a socket!");
        return -1;
    }

    /* be nice.. set SO_REUSEADDR */
    flg = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (int *)&flg, sizeof(flg)) < 0) {
        logger_log(WARN, "Could not set SO_REUSEADDR on socket!");
    }

    /* set all the options to listen to all interfaces; TODO customizeable */
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port   = htons(port);
    saddr.sin6_addr   = in6addr_any;

    /* bind port */
    if (bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        logger_log(ERR, "Could not bind to the port %d!", port);
        return -__LINE__;
    }

    /* listen and check for failures */
    if (listen(sfd, 1024) < 0) {
        logger_log(ERR, "Could not begin listening for network connections!");
        return -__LINE__;
    }

    /* set up epoll */
    if ((efd = epoll_create(32)) < 0) {
        logger_log(ERR, "Could not set up epoll!");
        return -__LINE__;
    }
    /* initially we only care about when the socket is ready to write to */
    epint.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP;

    /* set up pipe for serialization consumption */
    if (pipe(pfd) < 0) {
        logger_log(ERR, "Could not initialize pipe for workers!");
        return -__LINE__;
    }

    /* set up worker threads' handles */
    networkers = calloc(sizeof(*networkers), nwork);
    if (!networkers) {
        logger_log(ERR, "System OOM");
        return -__LINE__;
    }

    /* set up networkers */
    for (i = 0; i < nwork; i++) {
        /* set relevant variables */
        networkers[i].efd = efd;
        networkers[i].pfd = pfd[1]; /* write end */

        /* create nwork workers */
        if (pthread_create(&(networkers[i].thread), NULL, networker_loop, networkers + i)) {
            logger_log(ERR, "Failed to create worker thread");
            exit(-__LINE__);
        }
    }

    /* set up worker threads' handles */
    serworkers = calloc(sizeof(*serworkers), swork);
    if (!serworkers) {
        logger_log(ERR, "System OOM");
        return -__LINE__;
    }

    /* set up serializing workers */
    for (i = 0; i < swork; i++) {
        /* create swork workers */
        serworkers[i].pfd = pfd[0]; /* read end */
        if (pthread_create(&(serworkers[i].thread), NULL, serworker_loop, serworkers + i)) {
            logger_log(ERR, "Failed to create worker thread");
            exit(-__LINE__);
        }
    }

    /* loop forever! */
    while (running) {
        /* accept a connection */
        if ((cfd = accept(sfd, NULL, NULL)) < 0) {
            logger_log(ERR, "Failed to accept a connection!");
            break;
        }

        /* set this socket non-blocking */
        if ((flg = fcntl(cfd, F_GETFL, 0)) < 0) {
            logger_log(WARN, "Failed to set incoming socket nonblocking!");
            close(cfd);
            continue;
        }
        if (fcntl(cfd, F_SETFL, flg | O_NONBLOCK)) {
            logger_log(WARN, "Failed to set incoming socket nonblocking!");
            close(cfd);
            continue;
        }

        /* allocate memory for the client */
        if (!(epint.data.ptr = malloc(sizeof(struct client)))) {
            logger_log(WARN, "Server out of memory when accepting client!");
            close(cfd);
            continue;
        }
        ((struct client *)(epint.data.ptr))->cfd = cfd;
        ((struct client *)(epint.data.ptr))->state = S_BRANDNEW;
        ((struct client *)(epint.data.ptr))->bio = 0;

        /* add it to the epoll interest list */
        if (epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &epint) < 0) {
            logger_log(WARN, "Failed to initialize incoming connection!");
            free(epint.data.ptr);
            close(cfd);
            continue;
        }
    }

    return 0;
}
