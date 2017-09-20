/* This file has TODO's. */
#include "server.h"

char *server_greeting;
int server_greeting_len;

void server_initsocket(int *fd) {
    int on = 1;
    /* make a socket */
    if ((*fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, ERR"Could not create a socket!\n");
        return;
    }

    /* be nice.. set SO_REUSEADDR */
    if (setsockopt(*fd, SOL_SOCKET,
        SO_REUSEADDR, (int *)&on, sizeof(on)) < 0) {
        fprintf(stderr, WARN"Could not set SO_REUSEADDR on socket!\n");
    }
}

int server_bindport(int fd, int port) {
    struct sockaddr_in6 saddr;
    int res;

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port   = htons(port);
    saddr.sin6_addr   = in6addr_any;

    /* bind and check for failures */
    res = bind(fd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (res < 0) return res;

    /* listen and check for failures */
    res = listen(fd, 1024);
    return res;
}

void *server_child(void *arg) {
    /* vars */
    char buf_in[LARGEBUF], *srv;
    struct timeval timeout;
    enum server_stage stage;
    int rcn, fd;
    unsigned int buf_in_off;
    struct mail *mail;
    struct sockaddr_storage addr;

    /* initial values */
    timeout.tv_sec = 300;
    timeout.tv_usec = 0;
    buf_in_off = 0;
    mail = mail_new();
    stage = HELO;
    srv = NULL;

    /* extract the required data from args */
    fd = *((int *)arg);

    /* connection info */
    buf_in_off = sizeof(addr);
    getpeername(fd, (struct sockaddr *)&addr, &buf_in_off);
    if (mail) (mail->extra)->origin_ip = &addr;

    /* initial greeting */
    if (smtp_handlecode(220, fd)) goto disconnect;

    /* loops! yay! */
    while (1) {
        /* vars */
        fd_set sockset;
        char *eol, *lns;

        /* we use select() here to have a timeout */
        FD_ZERO(&sockset);
        FD_SET(fd, &sockset);

        select(fd + 1, &sockset, NULL, NULL, &timeout);

        /* check for timeout */
        if (!FD_ISSET(fd, &sockset)) {
            /* socket time out, close it */
            break;
        }

        /* receive data */
        rcn = recv(fd, buf_in + buf_in_off, LARGEBUF - buf_in_off - 1, 0);

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
                smtp_handlecode(500, fd);

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
                if (smtp_handlecode(smtp_parsel(lns, &stage, mail), fd))
                        goto disconnect;
                if (stage == MAIL)
                    srv = strdup(mail->froms_v);
            } else { /* processing message data */
                /* check for end of data */
                if (!strcmp(lns, ".")) {
                    /* end of data! send OK message and set status to MAIL */
                    smtp_handlecode(250, fd);
                    stage = MAIL;

                    /* print out this mail's info */
                    mail_serialize(mail, STDOUT, fd);

                    /* reset the mail */
                    mail_destroy(mail);
                    mail = mail_new();
                    if (mail) {
                        mail_setattr(mail, FROMS, srv);
                        (mail->extra)->origin_ip = &addr;
                    }
                } else {
                    /* change \0\n to \n\0 */
                    *eol = '\n';
                    *(eol + 1) = 0;
                    /* append data and check for errors */
                    rcn = mail_appenddata(mail, lns);
                    if (rcn == MAIL_ERROR_DATAMAX) {
                        smtp_handlecode(522, fd);
                    } else if (rcn == MAIL_ERROR_OOM) {
                        smtp_handlecode(451, fd);
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
            (mail->extra)->origin_ip = &addr;
            mail_serialize(mail, STDOUT, fd);
        }

        /* Clean up */
        mail_destroy(mail);
    }

    /* close socket */
    close(fd);
    free(arg);
    free(srv);
    pthread_exit(NULL);
}

int main() {
    /* vars */
    pthread_attr_t attr;
    pthread_t thread;
    int port, fd_s, *fd_c, fd_ctmp;

    /* user config */
    port = 25;

    /* initial socket setup and stuff */
    server_initsocket(&fd_s);
    if (fd_s < 0) return -1;

    if (smtp_gengreeting("midas.masonx.ca") < 0) {
        fprintf(stderr, ERR"System appears to be Out of Memory!\n");
        return -17;
    }

    /* bind port */
    if (server_bindport(fd_s, port) < 0) {
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
        /* accept() it */
        if ((fd_ctmp = accept(fd_s, NULL, NULL)) < 0) {
            fprintf(stderr, ERR"Failed to accept() a connection!\n");
            break;
        } else {
            /* allocate some memory for a new client session */
            fd_c = malloc(sizeof(int));

            /* Make sure it allocated correctly */
            if (fd_c == NULL) {
                /* just awkwardly close the connection if OOM */
                close(fd_ctmp);
                continue;
            }

            /* copy over value */
            *fd_c = fd_ctmp;

            /* accepted a new connection, create a thread! */
            pthread_create(&thread, &attr, &server_child, fd_c);
        }
    }

    return -1;
}
