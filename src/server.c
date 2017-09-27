#include "server.h"

char *server_greeting;
int server_greeting_len;
const char *server_hostname;
int server_hostname_len;
enum mail_sf server_sf;

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
    int rcn, fd, deliver_status;
    unsigned int buf_in_off, xaddr;
    struct mail *mail;
    struct sockaddr_storage addr;

    /* initial values */
    timeout.tv_sec = 300;
    timeout.tv_usec = 0;
    mail = mail_new();
    stage = HELO;
    srv = NULL;
    buf_in_off = 0;

    /* extract the required data from args */
    fd = *((int *)arg);

    /* connection info */
    xaddr = sizeof(addr);
    if (getpeername(fd, (struct sockaddr *)&addr, &xaddr) < 0) goto disconnect;
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
        buf_in[buf_in_off + rcn] = 0; /* null terminate the end string! :D */

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
                if (stage == MAIL && srv == NULL)
                    srv = strdup(mail->froms_v);
            } else { /* processing message data */
                /* check for end of data */
                if (!strcmp(lns, ".")) {
                    /* print out this mail's info (aka `deliver` it) */
                    deliver_status = mail_serialize(mail, server_sf);

                    /* end of data! send message and set status to MAIL */
                    if (!deliver_status) smtp_handlecode(250, fd);
                    else smtp_handlecode(422, fd);

                    /* end of data! send OK message and set status to MAIL */
                    
                    stage = MAIL;

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
    }
disconnect:
    if (mail != NULL) {
        /* make sure the email was filled */
        if (mail->from_c > 0) {
            (mail->extra)->origin_ip = &addr;
            mail_serialize(mail, server_sf);
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

void print_usage(const char *bin) {
    printf("mail version `"MAILVER"`\n");
    printf("Usage: %s <server address> [output format] [port]\n", bin);
    printf("\tserver address is the hostname or IP of the server\n");
    printf("\t\t(ie, `example.com` or `127.0.0.1`)\n");
    printf("\t\tMails that are not at the hostname will be rejected.\n");
    printf("\t\tTo allow mails to any dest server, specify the address '*'.\n");
    printf("\toutput format is how mail will be processed.\n");
    printf("\t\tSTDOUT - print out all emails, plus sender info, to STDOUT\n");
    printf("\t\tMBOX - output all emails in MBOX format to files\n");
    printf("\t\tBOTH - perform the operations of both STDOUT and MBOX\n");
    printf("\tport is the port to listen on (defaults to 25)\n");
}

int parse_arg(const char *v) {
    switch (*v) {
    case 'S':
        server_sf = STDOUT;
        break;
    case 'M':
        server_sf = MAILBOX;
        break;
    case 'B':
        server_sf = BOTH;
        break;
    default:
        return atoi(v);
    }
    return 0;
}

const char *sf_strs[5] = {"STDOUT", "ERR", "BINARY", "MAILBOX", "BOTH"};

int main(int argc, char **argv) {
    /* vars */
    pthread_attr_t attr;
    pthread_t thread;
    int fd_s, *fd_c, port;

    server_sf = BOTH;

    /* user config */
    if (argc == 2) { /* just server */
        port = 25;
    } else if (argc == 3) { /* server + port or server + string */
        port = parse_arg(argv[2]);
        if (port < 0 || port > 65535) {
            print_usage(argv[0]);
            return 64;
        }
        if (port == 0) port = 25; /* it was server + string */
    } else if (argc == 4) {
        if (parse_arg(argv[2]) != 0) { /* parse string */
            print_usage(argv[0]);
            return 64;
        }
        port = parse_arg(argv[3]);
        if (port < 0 || port > 65535) { /* parse port */
            print_usage(argv[0]);
            return 64;
        }
    } else { /* some error... */
        print_usage(argv[0]);
        return 64;
    }

    fprintf(stderr, INFO"%s starting!\n", MAILVER);
    fprintf(stderr, INFO"Accepting mails to `%s`; ", argv[1]);
    fprintf(stderr, "Listening on port %d; ", port);
    fprintf(stderr, "Output format `%s`\n", sf_strs[server_sf-1]);

    /* initial socket setup and stuff */
    server_initsocket(&fd_s);
    if (fd_s < 0) return -1;

    server_hostname = argv[1];
    server_hostname_len = strlen(server_hostname);

    if (smtp_gengreeting() < 0) {
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
        int fd_ctmp;
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
