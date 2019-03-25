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
/* struct worker */
#include "worker.h"

/* global variables */
struct barid_conf sconf;
sig_atomic_t running = 1;

/* main function */
int main(int argc, char **argv) {
    int sfd, efd, pfd, cfd, port, nwork, i, flg;
    struct worker *workers;
    struct epoll_event epint = {0};
    struct sockaddr_in6 saddr = {0};

    /* temp debug since configuration isn't set up */
    sconf.logger_fd = stderr;
    port = 8712;
    nwork = 2;

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
    efd = epoll_create(32);
    /* initially we only care about when the socket is ready to write to */
    epint.events = EPOLLOUT | EPOLLONESHOT;

    /* set up serialization workers */
    pfd = 0;

    /* set up worker threads' handles */
    workers = calloc(sizeof(*workers), nwork);
    if (!workers) {
        logger_log(ERR, "System OOM");
        return -__LINE__;
    }

    /* set up workers */
    for (i = 0; i < nwork; i++) {
        /* set relevant variables */
        workers[i].efd = efd;
        workers[i].pfd = pfd;

        /* create nwork workers */
        if (pthread_create(&(workers[i].thread), NULL, worker_loop, workers + i)) {
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
