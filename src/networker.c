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

    int rcn, ll, i, lc;
    unsigned char *lns, *lptr;
    enum known_verbs lverb;

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

        goto next_evt;
    }

    /* ??? programmer error?? */
    if (!(epevnt.events & EPOLLIN)) {
        logger_log(WARN, "wat %d", epevnt.events);
        goto next_evt;
    }

    /* for some states it is unsafe to perform a read right away */
    if (client->state == SSL_HS) {
        if (!net_sssl(client)) goto client_cleanup;
        else goto next_evt;
    }

    /* now we know we can read data safely */
    rcn = net_rx(client, buf, LARGEBUF - client->bio - 1);

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
            goto next_evt;
        default:
            goto client_cleanup;
        }
    }

    /* null-terminate data */
    client->buf[client->bio + rcn] = 0; /* null terminate for strstr safety */

    /* update the offset */
    client->bio += rcn;

    /* start from the start of the buffer */
    lns = client->buf;

next_line:
    if (!(lptr = strstr(lns, "\r\n")))
        goto parsing_done; /* done reading all lines */

    /* this line starts at lns and ends at lptr */
    ll = lptr - lns;

    /* convert verb to upper case if it's in lower case */
    for (i = 0; i < 4; i++) {
        if (line[i] >= 'a' && line[i] <= 'z') {
            line[i] -= 32;
        }
    }

    /* this is slightly faster than strcmp'ing each verb */
    lverb = UNKN;
    switch (*lns) {
    case 'D': if (!strncmp(lns, "DATA", 4)) lverb = DATA; break;
    case 'E': if (!strncmp(lns, "EHLO", 4)) lverb = EHLO; break;
    case 'H': if (!strncmp(lns, "HELO", 4)) lverb = HELO; break;
    case 'M': if (!strncmp(lns, "MAIL", 4)) lverb = MAIL; break;
    case 'N': if (!strncmp(lns, "NOOP", 4)) lverb = NOOP; break;
    case 'Q': if (!strncmp(lns, "QUIT", 4)) lverb = QUIT; break;
    case 'R': if (!strncmp(lns, "RSET", 4)) lverb = RSET;
              else if (!strncmp(lns, "RCPT", 4)) lverb = RCPT; break;
    case 'S': if (!strncmp(lns, "STARTTLS", 8)) lverb = STLS; break;
    }

    /* default = 250 OK */
    lc = 250;

    switch (lverb) {
    case NOOP: break;
    case UNKN: lc = 502; break; /* 502 command unknown */
    case EHLO: lc = 8250; /* 8250 is a custom code for EHLO response */
    case HELO:
        /* TODO: store the sending server */
        break;
    case STLS:
        if (!(sconf->flags & SSL_ENABLED)) { lc = 502; break; } /* 502 command unknown */
        if (client->state != MAIL) { lc = 503; break; } /* 503 wrong sequence */
        lc = 8220; /* 8220 is a custom code for starting TLS handshake */
        break;
    case MAIL:
    case RCPT:

    case DATA:
        
    case QUIT:
    case RSET:
        break;
    }

    /* send the code for this line */
    if (!smtp_handlecode(client, lc))
        /* error sending response */
        goto client_cleanup;
    else
        /* no problemo, go to next line */
        goto next_line;

    /* update our search to begin at the end of lptr */
    lns = lptr + 2;

    /* search for next line */
    goto next_line;

parsing_done:
    if (lns == client->buf) {
        if (client->bio + 1 >= LARGEBUF) {
            /* no newlines found and the data is at max length; this line is too long */
            /* TODO: send 500 */
            goto client_cleanup; /* TODO: quit or reset connection? */
        }
        /* no newlines here, but there's still room, so just read more. */
    } else {
        /* subtract how much we have consumed */
        client->bio -= (client->buf - lns);
        /* delete what has been consumed by shifting over the memory */
        memmove(client->buf, lns, client->bio);
    }

next_evt:
    /* resume listening for data from this client on the socket */
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
