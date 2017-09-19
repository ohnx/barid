/* This file has TODO's. */
#include "server.h"

void server_initsocket(struct server *state) {
    int on = 1;
    /* make a socket */
    if ((state->socket = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, ERR"Could not create a socket!\n");
        return;
    }

    /* be nice.. set SO_REUSEADDR */
    if (setsockopt(state->socket, SOL_SOCKET,
        SO_REUSEADDR, (int *)&on, sizeof(on)) < 0) {
        fprintf(stderr, WARN"Could not set SO_REUSEADDR on socket!\n");
    }
}

int server_bindport(struct server *state, int port) {
    struct sockaddr_in6 saddr;
    int res;

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port   = htons(port);
    saddr.sin6_addr   = in6addr_any;

    /* bind and check for failures */
    res = bind(state->socket, (struct sockaddr *)&saddr, sizeof(saddr));
    if (res < 0) return res;

    /* listen and check for failures */
    res = listen(state->socket, 1024);
    return res;
}

void *server_child(void *arg) {
    /* vars */
    struct session *sess;
    char buf_in[LARGEBUF], buf_out[SMALLBUF], *srv;
    struct timeval timeout;
    enum server_stage stage;
    int rcn;
    unsigned int buf_in_off, x;
    struct mail *mail;
    struct sockaddr_storage addr;

    /* initial values */
    timeout.tv_sec = 300;
    timeout.tv_usec = 0;
    buf_in_off = 0;
    mail = mail_new();
    stage = HELO;

    /* extract the required data from args */
    sess = (struct session *)arg;

    /* 
     * TODO: Not generate the greeting every. single. thread.
     * also TODO: move this to SMTP; ideally, server.c knows nothing about 
     * the SMTP protocol itself.
     */
    /* initial greeting */
    sprintf(buf_out, "220 %s %s\r\n", (sess->parent)->domain, MAILVER);
    send(sess->fd, buf_out, strlen(buf_out), 0);

    /* loops! yay! */
    while (1) {
        /* vars */
        fd_set sockset;
        char *eol, *lns;

        /* we use select() here to have a timeout */
        FD_ZERO(&sockset);
        FD_SET(sess->fd, &sockset);

        select(sess->fd + 1, &sockset, NULL, NULL, &timeout);

        /* check for timeout */
        if (!FD_ISSET(sess->fd, &sockset)) {
            /* socket time out, close it */
            break;
        }

        /* receive data */
        rcn = recv(sess->fd, buf_in + buf_in_off, LARGEBUF - buf_in_off - 1, 0);

        /* check for errors */
        if (rcn == 0) { /* Remote host closed connection */
			break;
		} else if (rcn == -1) { /* Error on socket */
			break;
		}

        /* null-terminate data */
        buf_in[rcn] = 0; /* null terminate the end string! :D */

        /* update the offset */
        lns = buf_in;
        buf_in_off += rcn;

        /* search for first newline */
        eol = strstr(buf_in, "\r\n");

        /* first newline not found */
        if (eol == NULL) {
            /* overflow; line too long */
            if (buf_in_off + rcn >= LARGEBUF - 1) {
                smtp_handlecode(500, sess->fd);

                /* reset the count and keep going */
                buf_in_off = 0;
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
                if (smtp_handlecode(smtp_parsel(lns, &stage, mail), sess->fd))
                        goto disconnect;
                if (stage == MAIL)
                    srv = strdup(mail->froms_v);
            } else { /* processing message data */
                /* check for end of data */
                if (!strcmp(lns, ".")) {
                    /* end of data! send OK message and set status to MAIL */
                    smtp_handlecode(250, sess->fd);
                    stage = MAIL;

                    /* print out this mail's info */
                    x = sizeof(addr);
                    /* Get server name */
                    getpeername(sess->fd, (struct sockaddr *)&addr, &x);
                    mail_serialize(mail, STDOUT, &addr, sess->fd);

                    /* reset the mail */
                    mail_destroy(mail);
                    mail = mail_new();
                    if (mail) mail_setattr(mail, FROMS, srv);
                } else {
                    /* change \0\n to \n\0 */
                    *eol = '\n';
                    *(eol + 1) = 0;
                    /* append data and check for errors */
                    rcn = mail_appenddata(mail, lns);
                    if (rcn == MAIL_ERROR_DATAMAX) {
                        smtp_handlecode(522, sess->fd);
                    } else if (rcn == MAIL_ERROR_OOM) {
                        smtp_handlecode(451, sess->fd);
                    }
                }
            }

            /* advance lns and eol and keep looking for more lines */
            eol += 2;
            lns = eol;
        } while ((eol = strstr(eol, "\r\n")) != NULL);

        /* update data offsets and buffer */
        memmove(buf_in, lns, LARGEBUF - (lns - buf_in));
        buf_in_off -= (lns - buf_in);

        /* do something magical that I have no idea what it does but it works */
        /* TODO: figure out what's going on here */
        if (stage == END_DATA) buf_in_off = 0;
    }
disconnect:
    if (mail != NULL) {
        /* make sure the email was filled */
        if (mail->from_c > 0) {
            /* reusing variable, sorry... */
            x = sizeof(addr);
            /* Get server name */
            getpeername(sess->fd, (struct sockaddr *)&addr, &x);
            mail_serialize(mail, STDOUT, &addr, sess->fd);
        }

        /* Clean up */
        mail_destroy(mail);
    }

    /* close socket */
    close(sess->fd);
    free(sess);
    free(srv);
    pthread_exit(NULL);
}

int main() {
    /* vars */
    pthread_attr_t attr;
    struct server state;
    int port;
    struct session *client_sess = NULL;

    /* user config */
    port = 25;

    /* initial socket setup and stuff */
    server_initsocket(&state);
    if (state.socket < 0) return -1;
    state.domain = "midas.masonx.ca";

    if (server_bindport(&state, port) < 0) {
        fprintf(stderr, ERR"Could not bind to the port %d!\n", port);
        return -2;
    }

    /* pthread config */
    if (pthread_attr_init(&attr) != 0) {
        fprintf(stderr, ERR"Failed to configure threads!\n");
        return -3;
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        fprintf(stderr, ERR"Failed to configure threads!\n");
        return -4;
    }

    /* loop forever! */
    while (1) {
        /* TODO: not exit on OOM; just keep accepting clients and send error */
        /* allocate some memory for a new client session */
        if (client_sess == NULL) {
            client_sess = malloc(sizeof(struct session));

            /* Make sure it allocated correctly */
            if (client_sess == NULL) {
                fprintf(stderr, ERR"System appears to be Out Of Memory!\n");
                return -19;
            }

            /* Initial values */
            client_sess->parent = &state;
            client_sess->data = NULL;
        }

        /* accept() it */
        if ((client_sess->fd = accept(state.socket, NULL, NULL)) < 0) {
            fprintf(stderr, ERR"Failed to accept() a connection!\n");
            break;
        } else {
            /* accepted a new connection, create a thread! */
            pthread_create(&(state.thread), &attr, &server_child, client_sess);

            /* require a new allocation for the next thread */
            client_sess = NULL;
        }
    }

    return -1;
}
