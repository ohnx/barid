/* this file does most of the real network i/o and processing */
/* free() */
#include <stdlib.h>
/* epoll-family functions */
#include <sys/epoll.h>
/* strstr(), strncmp() */
#include <string.h>

/* DEBUG TODO */
#include <stdio.h>

/* mbedtls_ssl_free */
#include "mbedtls/ssl.h"

/* running flag */
#include "common.h"
/* struct networker */
#include "networker.h"
/* serworker_deliver() */
#include "serworker.h"
/* mail family functions */
#include "mail.h"
/* smtp_handlecode() */
#include "smtp.h"
/* net family functions */
#include "net.h"
/* logger_log() */
#include "logger.h"

#define xstr(s) str(s)
#define str(s) #s

void *networker_loop(void *z) {
    struct networker *self = (struct networker *)z;
    struct epoll_event epevnt;
    struct client *client;
    /* TODO: remove need for tmp pointer by adding instantiator for mail to require remote server address */
    void *tmp;

    int rcn, ll, i, lc;
    unsigned char *lns, *lptr;
    enum known_verbs lverb;

start:
    if (epoll_wait(self->efd, &epevnt, 1, -1) < 0) goto end;
    client = (struct client *)(epevnt.data.ptr);

    /* error occurred */
    if ((epevnt.events & EPOLLRDHUP) ||
        (epevnt.events & EPOLLERR) ||
        (epevnt.events & EPOLLHUP)) goto client_cleanup;

    if ((epevnt.events & EPOLLOUT) && client->state == S_BRANDNEW) {
        /* brand new connection */
        logger_log(INFO, "Client %d connected!", client->cfd);
        smtp_handlecode(client, 220);

        /* update stage */
        client->state = S_HELO;

        goto next_evt;
    }

    /* ??? programmer error?? */
    if (!(epevnt.events & EPOLLIN) && !(epevnt.events & EPOLLPRI)) {
        logger_log(WARN, "wat %d", epevnt.events);
        goto next_evt;
    }

    /* for some states it is unsafe to perform a read right away */
    if (client->state == S_SSL_HS) {
        /* in the middle of an SSL handshake */
        printf("RESUMING SSL HANDSHAKE\n");
        if (net_sssl(client)) goto client_cleanup;
        else goto next_evt;
    }

    /* now we know we can read data safely */
    rcn = net_rx(client);

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
            /* no error, just need to read some more data */
            goto next_evt;
        default:
            printf("DATA READ ERROR OCCURRED!\n");
            goto client_cleanup;
        }
    }

    /* null-terminate data */
    client->buf[client->bio + rcn] = 0; /* null terminate for strstr safety */

    /* update the offset */
    client->bio += rcn;

    /* start from the start of the buffer */
    lns = client->buf;
    lptr = NULL;

next_line:
    /* update our search to begin at the end of lptr */
    if (lptr) lns = lptr + 2;

    if (!(lptr = (unsigned char *)strstr((char *)lns, "\r\n")))
        goto parsing_done; /* done reading all lines */

    /* this line starts at lns and ends at lptr */
    ll = lptr - lns;

    /* we don't really want the newlines here */
    *lptr = '\0';

    /* Check if we are in data mode */
    if (client->state == S_END_DATA) {
        /* change \0\n to \n\0 for data appending purposes */
        *lptr = '\n';
        *(lptr + 1) = '\0';

        if (*lns == '.' && lns[1] == '\n') {
            serworker_deliver(self->pfd, client->mail);
            tmp = mail_new();
            mail_setattr((struct mail *)tmp, FROMS, client->mail->froms_v);
            client->mail = (struct mail *)tmp;
            client->state = S_MAIL;
            lc = 250;
            goto line_handle_code;
        }

        i = mail_appenddata(client->mail, (char *)(lns));
        switch (i) {
        case MAIL_ERROR_DATAMAX: smtp_handlecode(client, 522);
        case MAIL_ERROR_OOM: smtp_handlecode(client, 451);
        }
        goto next_line;
    }

    /* convert verb to upper case if it's in lower case */
    for (i = 0; i < 4; i++) {
        if (lns[i] >= 'a' && lns[i] <= 'z') {
            lns[i] -= 32;
        }
    }

    /* this is slightly faster than strcmp'ing each verb, i think */
    lverb = V_UNKN;
    switch (*lns) {
    case 'D': if (!strncmp((char *)lns, "DATA", 4)) lverb = V_DATA; break;
    case 'E': if (!strncmp((char *)lns, "EHLO", 4)) lverb = V_EHLO; break;
    case 'H': if (!strncmp((char *)lns, "HELO", 4)) lverb = V_HELO; break;
    case 'M': if (!strncmp((char *)lns, "MAIL", 4)) lverb = V_MAIL; break;
    case 'N': if (!strncmp((char *)lns, "NOOP", 4)) lverb = V_NOOP; break;
    case 'Q': if (!strncmp((char *)lns, "QUIT", 4)) lverb = V_QUIT; break;
    case 'R': if (!strncmp((char *)lns, "RSET", 4)) lverb = V_RSET;
              else if (!strncmp((char *)lns, "RCPT", 4)) lverb = V_RCPT;
              break;
    case 'S': if (!strncmp((char *)lns, "STARTTLS", 8)) lverb = V_STLS; break;
    }

    /* default = 250 OK */
    lc = 250;

    switch (lverb) {
    case V_NOOP: break;
    case V_UNKN: lc = 502; break; /* 502 command unknown */
    case V_QUIT: lc = 221; break;
    case V_RSET:
        /* reset the mail */
        if (client->mail) {
            mail_reset(client->mail);
            client->state = S_MAIL;
        } else {
            client->state = S_HELO;
        }

        lc = 250;
        break;
    case V_EHLO:
    case V_HELO:
        if (client->state != S_HELO) { lc = 503; break; } /* 503 wrong sequence */
        if (ll < 6) {
            /* line too short - no way to have a server name here */
            lc = 501;
            break;
        }

        /* initialize the mail struct */
        client->mail = mail_new();
        if (!(client->mail)) {
            logger_log(WARN, "Server out of memory!");
            goto client_cleanup;
        }

        /* store the server in the mail struct */
        i = mail_setattr(client->mail, FROMS, (char *)(lns + 5)); /* +5 skips EHLO */
        switch (i) {
        case MAIL_ERROR_PARSE: lc = 501; break;
        case MAIL_ERROR_OOM: lc = 451; break;
        case MAIL_ERROR_NONE:
            if (lverb == V_EHLO) lc = 8250; /* 8250 is a custom code for EHLO response */
            else lc = 250;

            /* update state */
            client->state = S_MAIL;
            break;
        default: logger_log(ERR, "Uncaught exception "__FILE__ ":" xstr(__LINE__) "" MAILVER); goto end;
        }
        break;
    case V_STLS:
        if (!(self->sconf->ssl_enabled)) { lc = 502; break; } /* 502 command unknown */
        if (client->state != S_MAIL) { lc = 503; break; } /* 503 wrong sequence */
        lc = 8220; /* 8220 is a custom code for starting TLS handshake */
        break;
    case V_MAIL:
        if (client->state != S_MAIL) { lc = 503; break; } /* 503 wrong sequence */

        /* ensure valid MAIL FROM: message */
        if (ll < 10) { lc = 501; break; } /* 501 argument error */

        /* skip past whitespace */
        for (i = 10; i < ll; i++)
            if (lns[i] != ' ') break;

        /* store the address from in the mail struct */
        i = mail_setattr(client->mail, FROM, (char *)(lns + i));
        switch (i) {
        case MAIL_ERROR_PARSE: lc = 501; break;
        case MAIL_ERROR_OOM: lc = 451; break;
        case MAIL_ERROR_NONE:
            lc = 250;
            /* update state */
            client->state = S_RCPT; break;
        default: logger_log(ERR, "Uncaught exception "__FILE__ ":" xstr(__LINE__) "" MAILVER); goto end;
        }
        break;
    case V_RCPT:
        if (client->state != S_RCPT && client->state != S_DATA)
            { lc = 503; break; } /* 503 wrong sequence */

        /* ensure valid RCPT TO: message */
        if (ll < 8) { lc = 501; break; } /* 501 argument error */

        /* skip past whitespace */
        for (i = 8; i < ll; i++)
            if (lns[i] != ' ') break;

        /* add another mail recipient */
        i = mail_addattr(client->mail, TO, (char *)(lns + i));
        switch (i) {
        case MAIL_ERROR_PARSE: lc = 501; break;
        case MAIL_ERROR_OOM: lc = 451; break;
        case MAIL_ERROR_INVALIDEMAIL: lc = 450; break;
        case MAIL_ERROR_RCPTMAX: lc = 452; break;
        case MAIL_ERROR_USRNOTLOCAL: lc = 550; break;
        case MAIL_ERROR_NONE:
            lc = 250;
            /* update state */
            client->state = S_DATA; break;
        default: logger_log(ERR, "Uncaught exception "__FILE__ ":" xstr(__LINE__) "" MAILVER); goto end;
        }
        break;
    case V_DATA:
        if (client->state != S_DATA) { lc = 503; break; } /* 503 wrong sequence */

        /* update state */
        client->state = S_END_DATA;

        lc = 354; /* 354 CONTINUE */
        break;
    }

line_handle_code:
    /* send the code for this line */
    if (smtp_handlecode(client, lc))
        /* error sending response */
        goto client_cleanup;
    else
        /* no problemo, go to next line */
        goto next_line;

    /* search for next line */
    goto next_line;

parsing_done:
    if (lns == client->buf) {
        if (client->bio + 1 >= LARGEBUF) {
            /* no newlines found and the data is at max length; this line is too long */
            smtp_handlecode(client, 500);

            client->bio = 0;
            goto next_evt;
        }
        /* no newlines here, but there's still room, so just read more. */
    } else {
        /* subtract how much we have consumed */
        client->bio -= (lns - client->buf);
        /* shift over the unused memory */
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
    logger_log(INFO, "Client %d disconnected!", client->cfd);
    net_close(client);
    if (client->mail) mail_destroy(client->mail);
    free(client);
    goto start;
}
