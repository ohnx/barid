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

/**
 * I've hit a roadblock here.
 * 
 * My issue is that when I read in a 8192 bytes, I don't know when the line
 * will end.
 * 
 * I can resolve this if each client has a running buffer that I use.
 * 
 * I guess that is doable.
 */

void *networker_loop(void *z) {
    struct networker *self = (struct networker *)z;
    struct epoll_event epevnt;
    struct client *client;
    char *lns;
    int rcn;

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

    /* ??? */
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
    client->len = net_rx(client, buf, LARGEBUF);

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

    /* null-terminate data */
    client->buf[client->bio + rcn] = 0; /* null terminate the end string! :D */

    /* update the offset */
    lns = client->buf;
    client->bio += rcn;

    /* search for first newline */
    eol = strstr(client->buf, "\r\n");

    /* first newline not found */
    if (eol == NULL) {
        /* overflow; line too long */
        if (client->bio + rcn >= LARGEBUF - 1) {
            smtp_handlecode(500, client);

            /* reset the count and keep going */
            client->bio = 0;
            continue;
        } else {
            /* keep expecting more data */
            continue;
        }
    }

    /* keep parsing lines */
    do {
        /* Null-terminate this line */
        *eol = 0;

        /* are we processing verbs or what? */
        if (stage != END_DATA) { /* processing a verb */
            /* smtp_handlecode returns 1 when server should quit */
            if (smtp_handlecode(smtp_parsel(lns, &stage, mail), conn))
                    goto client_cleanup;
            if (stage == MAIL && srv == NULL)
                srv = _m_strdup(mail->froms_v);
        } else { /* processing message data */
            /* check for end of data */
            if (!strcmp(lns, ".")) {
                /* `deliver` this mail */
                deliver_status = mail_serialize(mail, server_sf);

                /* end of data! send message and set status to MAIL */
                if (!deliver_status) smtp_handlecode(250, conn);
                else smtp_handlecode(422, conn);

                /* end of data! send OK message and set status to MAIL */
                stage = MAIL;

                /* reset the mail */
                mail_reset(mail);
            } else {
                /* change \0\n to \n\0 */
                *eol = '\n';
                *(eol + 1) = 0;
                /* append data and check for errors */
                rcn = mail_appenddata(mail, lns);
                if (rcn == MAIL_ERROR_DATAMAX) {
                    smtp_handlecode(522, conn);
                } else if (rcn == MAIL_ERROR_OOM) {
                    smtp_handlecode(451, conn);
                }
            }
        }

        /* advance lns and eol and keep looking for more lines */
        eol += 2;
        lns = eol;
    } while ((eol = strstr(eol, "\r\n")) != NULL);

    /* update data offsets and buffer */
    memmove(client->buf, lns, LARGEBUF - (lns - client->buf));
    client->bio -= (lns - client->buf);


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
