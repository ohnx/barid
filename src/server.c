/* this file reads in the configuration and initializes sockets and stuff */
/* malloc() */
#include <stdlib.h>
/* strcmp(), memcpy() */
#include <string.h>
/* socket(), setsockopt(), bind(), listen() */
#include <sys/socket.h>
/* struct sockaddr_in6 */
#include <netinet/in.h>
/* inet_pton() */
#include <arpa/inet.h>
/* fnctl() */
#include <unistd.h>
/* fnctl() */
#include <fcntl.h>
/* errno */
#include <errno.h>
/* sig_atomic_t, struct sigaction, sigaction(), etc. */
#include <signal.h>
/* epoll-family functions */
#include <sys/epoll.h>
/* clone() and associated flags */
#include <sched.h>
/* waitpid() */
#include <sys/wait.h>
/* ini_parse */
#include "ini.h"
/* SPF_server_new(), SPF_server_free() */
#include "spf.h"

/* MAIL_VER, struct barid_conf */
#include "common.h"
/* net_init_ssl() */
#include "net.h"
/* logger_log() */
#include "logger.h"
/* struct networker */
#include "networker.h"
/* struct serworker */
#include "serworker.h"
/* smtp_gendynamic(), smtp_freedynamic() */
#include "smtp.h"
/* mail_set_allowed() */
#include "mail.h"

/* global variables */
sig_atomic_t running = 1;


/* stack size for each child process */
#define CHILD_STACKSIZE         65536

#ifndef USE_PTHREADS
/* flags to the clone call */
/*
 * CLONE_FILES to share file descriptors, CLONE_SYSVSEM because why not,
 * CLONE_VM to share memory
 */
#define CLONE_FLAGS (CLONE_FILES | CLONE_SYSVSEM | CLONE_VM)
#endif

/* configuration parser */
static int server_conf(void *c, const char *s, const char *k, const char *v) {
    struct barid_conf *sconf = (struct barid_conf *)c;
    char extra[23];
    int z;

    if (!strcmp(s, "general")) {
        if (!strcmp(k, "host") && !sconf->host) sconf->host = strdup(v);
        else if (!strcmp(k, "domains") && !sconf->domains) sconf->domains = strdup(v);
        else if (!strcmp(k, "accounts") && !sconf->accounts) sconf->accounts = strdup(v);
    } else if (!strcmp(s, "workers")) {
        if (!strcmp(k, "network")) {
            sconf->network = atoi(v);
            if (sconf->network <= 0 || sconf->network > 128) {
                sconf->network = 2;
                logger_log(WARN, "Invalid number of network workers, using default %d", sconf->network);
            }
        } else if (!strcmp(k, "delivery")) {
            sconf->delivery = atoi(v);
            if (sconf->delivery <= 0 || sconf->delivery > 128) {
                sconf->delivery = 2;
                logger_log(WARN, "Invalid number of delivery workers, using default %d", sconf->delivery);
            }
        }
    } else if (!strcmp(s, "network")) {
        if (!strcmp(k, "bind")) {
            /* check if this is ipv4 formatted */
            extra[22] = strlen(v);
            extra[2] = 0;
            for (*extra = 0; *extra < extra[22]; (*extra)++) {
                if (v[(int)*extra] == '.') { extra[2]++; }
                if (v[(int)*extra] == ':') { extra[2]++; }
            }

            /* ipv4 addresses would have exactly 3 dots */
            if (extra[22] < 16 && extra[2] == 3) {
                /* prefix ::ffff: */
                extra[0] = ':'; extra[1] = ':'; extra[2] = 'f'; extra[3] = 'f';
                extra[4] = 'f'; extra[5] = 'f'; extra[6] = ':';

                /* to the ipv4 address (include null) */
                memcpy(extra+7, v, extra[22]+1);

                /* then convert */
                if (inet_pton(AF_INET6, extra, &(sconf->bind.sin6_addr)) <= 0) {
                    sconf->bind.sin6_addr = in6addr_any;
                    logger_log(WARN, "Invalid bind address %s, using default ANY", extra);
                }
            } else if (inet_pton(AF_INET6, v, &(sconf->bind.sin6_addr)) <= 0) {
                sconf->bind.sin6_addr = in6addr_any;
                logger_log(WARN, "Invalid bind address, using default ANY");
            }
        } else if (!strcmp(k, "port")) {
            z = atoi(v);
            if (z < 1 || z > 65535) {
                z = 2525;
                logger_log(WARN, "Invalid port, using default %d", z);
            }
            sconf->bind.sin6_port = htons((short)z);
        }
    } else if (!strcmp(s, "ssl")) {
        if (!strcmp(k, "key") && !sconf->ssl_key) {
            sconf->ssl_key = strdup(v);
        } else if (!strcmp(k, "cert") && !sconf->ssl_cert) {
            sconf->ssl_cert = strdup(v);
        }

        /* configure SSL if both key and cert specified */
        if (sconf->ssl_key && sconf->ssl_cert) {
            if (net_init_ssl(sconf->ssl_cert, sconf->ssl_key)) {
                logger_log(WARN, "SSL disabled due to configuration error");
            } else {
                sconf->ssl_enabled = 1;
            }
            free(sconf->ssl_key);
            free(sconf->ssl_cert);
            sconf->ssl_key = NULL;
            sconf->ssl_cert = NULL;
        }
    } else if (!strcmp(s, "delivery")) {
        if (!strcmp(k, "enable_spf")) {
            if (!strcmp(v, "true") || !strcmp(v, "1")) {
#ifdef DEBUG
                sconf->spf_server = SPF_server_new(SPF_DNS_CACHE, 1);
#else
                sconf->spf_server = SPF_server_new(SPF_DNS_CACHE, 0);
#endif
                if (!sconf->spf_server) {
                    logger_log(WARN, "SPF disabled due to setup error");
                }
            } else if (!strcmp(v, "false") || !strcmp(v, "0")) {
                sconf->spf_server = NULL;
            } else {
                logger_log(WARN, "Invalid option for enable_spf in delivery, using default true");
#ifdef DEBUG
                sconf->spf_server = SPF_server_new(SPF_DNS_CACHE, 1);
#else
                sconf->spf_server = SPF_server_new(SPF_DNS_CACHE, 0);
#endif
                if (!sconf->spf_server) {
                    logger_log(WARN, "SPF disabled due to setup error");
                }
            }
        } else if (!strcmp(k, "mode")) {
            if (!strcmp(v, "mbox")) {
                sconf->delivery_mode = DELIVER_MBOX;
            } else if (!strcmp(v, "maildir")) {
                sconf->delivery_mode = DELIVER_MAILDIR;
            } else {
                logger_log(WARN, "Invalid option for mode in delivery, using default mbox");
                sconf->delivery_mode = DELIVER_MBOX;
            }
        }
    }

    return 1;
}

/* signal handler */
static void cleanup(int signum) {
    if (signum != SIGHUP) running = 0;
}

/* main function */
int main(int argc, char **argv) {
    int sfd, efd, pfd[2], cfd, i, flg;
    struct barid_conf *config;
    struct networker *networkers;
    struct serworker *serworkers;
    struct epoll_event epint = {0};
    struct sigaction action;

    config = calloc(sizeof(struct barid_conf), 1);

    /* hi, world! */
#ifndef DEBUG
    logger_log(INFO, "%s starting!", MAILVER);
#else
    logger_log(WARN, "debugging %s!", MAILVER);
#endif

    /* load configuration */
    config->bind.sin6_family = AF_INET6;
    if (ini_parse(argc == 2 ? argv[1] : "barid.ini", server_conf, config) < 0) {
        logger_log(ERR, "Could not load configuration file %s", argc == 2 ? argv[1] : "barid.ini");
        return -__LINE__;
    }

    /* config defaults */
    if (config->network <= 0) config->network = 2;
    if (config->delivery <= 0) config->delivery = 2;
    if (!config->host) config->host = strdup("localhost");
    if (!config->domains) config->host = strdup("*");
    if (!config->accounts) config->host = strdup("*");

    /* setup the smtp greetings and stuff */
    smtp_gendynamic(config);

    /* setup the allowed email addresses */
    mail_set_allowed(config);

    /* start by creating a socket */
    if ((sfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        logger_log(ERR, "Could not create a socket!");
        return -__LINE__;
    }

    /* be nice.. set SO_REUSEADDR */
    flg = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (int *)&flg, sizeof(flg)) < 0) {
        logger_log(WARN, "Could not set SO_REUSEADDR on socket!");
    }

    /* bind port */
    if (bind(sfd, (struct sockaddr *)&(config->bind), sizeof(config->bind)) < 0) {
        logger_log(ERR, "Could not bind to the port %d!", ntohs(config->bind.sin6_port));
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
    networkers = calloc(sizeof(*networkers), config->network);
    if (!networkers) {
        logger_log(ERR, "System OOM");
        return -__LINE__;
    }

    /* set up networkers */
    for (i = 0; i < config->network; i++) {
        /* set relevant variables */
        networkers[i].efd = efd;
        networkers[i].pfd = pfd[1]; /* write end */
        networkers[i].sconf = config;

#ifndef USE_PTHREADS
        /* allocate stack for child */
        networkers[i].stack = malloc(CHILD_STACKSIZE);
        if (!networkers[i].stack) {
            logger_log(ERR, "Failed to create worker thread");
            return -__LINE__;
        }

        /* create new process with shared memory and running the networker_loop */
        networkers[i].pid = clone(networker_loop,
                                    (unsigned char *)networkers[i].stack + CHILD_STACKSIZE,
                                    CLONE_FLAGS,
                                    networkers + i
                                    );
        if (networkers[i].pid < 0) {
            logger_log(ERR, "Failed to create worker thread: %s", strerror(errno));
            return -__LINE__;
        }
#else
        if (pthread_create(&(networkers[i].thread), NULL, networker_loop, networkers + i)) {
            logger_log(ERR, "Failed to create worker thread");
            exit(-__LINE__);
        }
#endif
    }

    /* set up worker threads' handles */
    serworkers = calloc(sizeof(*serworkers), config->delivery);
    if (!serworkers) {
        logger_log(ERR, "System OOM");
        return -__LINE__;
    }

    /* set up serializing workers */
    for (i = 0; i < config->delivery; i++) {
        /* create swork workers */
        serworkers[i].pfd = pfd[0]; /* read end */
        serworkers[i].sconf = config;

#ifndef USE_PTHREADS
        /* allocate stack for child */
        serworkers[i].stack = malloc(CHILD_STACKSIZE);
        if (!serworkers[i].stack) {
            logger_log(ERR, "Failed to create worker thread");
            return -__LINE__;
        }

        /* create new process with shared memory and running the serworker_loop */
        serworkers[i].pid = clone(serworker_loop,
                                    (unsigned char *)serworkers[i].stack + CHILD_STACKSIZE,
                                    CLONE_FLAGS,
                                    serworkers + i
                                    );
        if (serworkers[i].pid < 0) {
            logger_log(ERR, "Failed to create worker thread: %s", strerror(errno));
            return -__LINE__;
        }
#else
        if (pthread_create(&(serworkers[i].thread), NULL, serworker_loop, serworkers + i)) {
            logger_log(ERR, "Failed to create worker thread");
            exit(-__LINE__);
        }
#endif
    }

    /* catch signals */
    memset(&action, 0, sizeof(struct sigaction));
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_NODEFER;
    /* cleanup after sigterm, sigint, and sighup */
    action.sa_handler = cleanup;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
    /* ignore sigpipe */
    action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &action, NULL);

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
        ((struct client *)(epint.data.ptr))->ssl = NULL;
        ((struct client *)(epint.data.ptr))->mail = NULL;

        /* add it to the epoll interest list */
        if (epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &epint) < 0) {
            logger_log(WARN, "Failed to initialize incoming connection!");
            free(epint.data.ptr);
            close(cfd);
            continue;
        }
    }

    /* cleanup */
    logger_log(ERR, "%s quitting!", MAILVER);

    for (int i = 0; i < config->network; i++) {
#ifndef USE_PTHREADS
        /* before clearing the stacks, waitpid just in case */
        waitpid(networkers[i].pid, NULL, 0);
        free(networkers[i].stack);
#else
        pthread_kill(networkers[i].thread, SIGTERM);
        pthread_join(networkers[i].thread, NULL);
#endif
    }
    free(networkers);

    for (int i = 0; i < config->delivery; i++) {
#ifndef USE_PTHREADS
        /* before clearing the stacks, waitpid just in case */
        waitpid(serworkers[i].pid, NULL, 0);
        free(serworkers[i].stack);
#else
        pthread_kill(serworkers[i].thread, SIGTERM);
        pthread_join(serworkers[i].thread, NULL);
#endif
    }
    free(serworkers);

    net_deinit_ssl();
    smtp_freedynamic();

    free(config->host);
    free(config->domains);
    free(config->accounts);
    if (config->spf_server) {
        SPF_server_free(config->spf_server);
        config->spf_server = NULL;
    }

    free(config);

    return 0;
}
