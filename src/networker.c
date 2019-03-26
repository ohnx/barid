/* this file does most of the real network i/o and processing */
/* free() */
#include <stdlib.h>
/* epoll-family functions */
#include <sys/epoll.h>

/* mbedtls_ssl_free */
#include "mbedtls/ssl.h"

/* running flag */
#include "common.h"
/* struct networker */
#include "networker.h"
/* serworker_deliver() */
#include "serworker.h"
/* net family functions */
#include "net.h"
/* logger_log() */
#include "logger.h"

void *networker_loop(void *z) {
    struct networker *self = (struct networker *)z;
    struct epoll_event epevnt;
    struct client *client;
    unsigned char buf[LARGEBUF];
    int len;
    unsigned int bio;

start:
    if (epoll_wait(self->efd, &epevnt, 1, -1) < 0) goto end;
    client = (struct client *)(epevnt.data.ptr);

    /* error occurred */
    if ((epevnt.events & EPOLLRDHUP) ||
        (epevnt.events & EPOLLERR) ||
        (epevnt.events & EPOLLHUP)) {
        logger_log(INFO, "Client %d disconnected!", client->cfd);
        goto client_cleanup;
    }

    if ((epevnt.events & EPOLLOUT) && client->state == BRANDNEW) {
        /* brand new connection */
        logger_log(INFO, "Client %d connected!", client->cfd);
        net_tx(client, (unsigned char *)"hello!\n", 7);

        /* update stage */
        client->state = HELO;

        goto next;
    }

    if (!(epevnt.events & EPOLLIN)) {
        logger_log(WARN, "wat %d", epevnt.events);
        goto next;
    }

    /* for some states it is unsafe to perform a read right away */
    if (client->state == SSL_HS) {
        if (!net_sssl(client)) goto client_cleanup;
        else goto next;
    }

    /* now we can read data safely */
    len = net_rx(client, buf, LARGEBUF);

    /* check for errors */
    if (rcn == 0) { /* Remote host closed connection */
		goto client_cleanup;
	} else if (rcn < 0) { /* Error on socket */
	    switch (rcn) {
	    case MBEDTLS_ERR_SSL_WANT_READ:
	    case MBEDTLS_ERR_SSL_WANT_WRITE:
	    case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS:
	    case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS:
	    case MBEDTLS_ERR_SSL_CLIENT_RECONNECT:
	        goto next;
	    default:
	        goto client_cleanup;
	    }
	}

    /* */

    switch(client->state) {
    case SSL_HS:
        /* keep trying to handshake */
        if (!net_sssl(client)) goto client_cleanup;
        break;
    case HELO:
        
    default:
        len = net_rx(client, buf, 4096);
        if (*buf == '*') {
            serworker_deliver(self->pfd, (struct mail *)(1));
        }
        net_tx(client, buf, len);
        break;
    }

next:
    /* resume listening for data from the socket */
    epevnt.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP;
    if (epoll_ctl(self->efd, EPOLL_CTL_MOD, client->cfd, &epevnt) < 0) {
        logger_log(WARN, "Failed to listen to client %d!", client->cfd);
        goto client_cleanup;
    }

    if (!running) goto end;
    else goto start;

end:
    return NULL;

client_cleanup:
    /* clean up client data */
    net_close(client);
    free(client);
    goto start;
}
