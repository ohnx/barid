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
    res = listen(state->socket, 16);
    return res;
}

void *server_child(void *arg) {
    /* vars */
    struct session *sess;
    char buf_in[LARGEBUF], buf_out[SMALLBUF];
    struct timeval timeout;
    int recvnum, buf_in_off, i;
    char flg_in_data = 0;

    /* initial values */
    timeout.tv_sec = 120;
    timeout.tv_usec = 0;
    buf_in_off = 0;

    /* extract the required data from args */
    sess = (struct session *)arg;

    /* TODO: Not generate the greeting every. single. thread. */
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
        recvnum = recv(sess->fd, buf_in + buf_in_off, LARGEBUF - buf_in_off, 0);

        printf("got data from %d: ```\n%s\n```\n", sess->fd, buf_in);

        /* check for errors */
        if (recvnum == 0) { /* Remote host closed connection */
			break;
		} else if (recvnum == -1) { /* Error on socket */
			break;
		}

        /* update the offset */
        lns = buf_in;
        buf_in_off += recvnum;

        /* parse the data */
        eol = strstr(buf_in, "\r\n");

        /* eol not found */
        if (eol == NULL) {
            /* overflow; line too long */
            if (buf_in_off >= LARGEBUF) {
                send(sess->fd, "500 Line too long\r\n", 19, 0);

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
            if (!flg_in_data) { /* processing a verb */
                /* convert to upper case if it's in lower case */
                for (i = 0; i < 4; i++) {
                    if (lns[i] >= 'a' && lns[i] < 'z') {
                        lns[i] -= 32;
                    }
                }

                /* check verbs */
                if (!strncmp(lns, "HELO", 4)) { /* initial greeting */
                    send(sess->fd, "250 OK\r\n", 8, 0);
                } else if (!strncmp(lns, "MAIL", 4)) { /* got mail from ... */
                    send(sess->fd, "250 OK\r\n", 8, 0);
                } else if (!strncmp(lns, "RCPT", 4)) { /* mail addressed to */
                    send(sess->fd, "250 OK RCPT\r\n", 13, 0);
                } else if (!strncmp(lns, "DATA", 4)) { /* message data */
                    send(sess->fd, "354 CONTINUE\r\n", 14, 0);
                    flg_in_data = 1;
                } else if (!strncmp(lns, "NOOP", 4)) { /* do nothing */
                    send(sess->fd, "250 OK\r\n", 8, 0);
                } else if (!strncmp(lns, "RSET", 4)) { /* reset connection */
                    send(sess->fd, "250 OK\r\n", 8, 0);
                } else if (!strncmp(lns, "QUIT", 4)) { /* bye! */
                    send(sess->fd, "221 OK\r\n", 8, 0);
                    goto disconnect;
                } else {
                    send(sess->fd, "502 Command not implemented\r\n", 29, 0);
                }
            } else { /* processing message data */
                if (strstr(lns, ".")) {
                    send(sess->fd, "250 OK\r\n", 8, 0);
                    flg_in_data = 0;
                }
            }

            /* advance lns and eol and keep looking for more lines */
            eol += 2;
            lns = eol;
        } while ((eol = strstr(eol, "\r\n")) != NULL);

        /* update data offsets and buffer */
        memmove(buf_in, lns, LARGEBUF - (lns - buf_in));
        buf_in_off -= (lns - buf_in);
        
        if (flg_in_data) {
            buf_in_off = 0;
        }
    }

disconnect:
    /* close socket */
    close(sess->fd);
    free(sess);
    pthread_exit(NULL);
}

int main() {
    /* vars */
    struct server state;
    int port;
    struct session *client_sess;

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

    /* loop forever! */
    while (1) {
        /* allocate some memory for a new client session */
        if (client_sess == NULL) {
            client_sess = malloc(sizeof(client_sess));

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
            pthread_create(&(state.thread), NULL, &server_child, client_sess);

            /* require a new allocation for the next thread */
            client_sess = NULL;
        }
    }

    return -1;
}
