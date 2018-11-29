#include "server.h"

char *server_greeting;
int server_greeting_len;
char *server_hostname;
enum mail_sf server_sf;
volatile sig_atomic_t prgrm_r = 1;

static char *_m_strdup(const char *x) {
    int l = strlen(x) + 1;
    char *p = malloc(l*sizeof(char));
    memcpy(p, x, l);
    return p;
}

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
    int rcn, deliver_status;
    unsigned int buf_in_off, xaddr;
    struct connection *conn;
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
    conn = (struct connection *)arg;

    /* connection info */
    xaddr = sizeof(addr);
    if (getpeername(conn->fd, (struct sockaddr *)&addr, &xaddr) < 0) goto disconnect;
    if (mail) (mail->extra)->origin_ip = &addr;

    /* initial greeting */
    if (smtp_handlecode(220, conn)) goto disconnect;

    /* loops! yay! */
    while (1) {
        /* vars */
        fd_set sockset;
        char *eol, *lns;

        /* we use select() here to have a timeout */
        FD_ZERO(&sockset);
        FD_SET(conn->fd, &sockset);

        select(conn->fd + 1, &sockset, NULL, NULL, &timeout);

        /* check for timeout */
        if (!FD_ISSET(conn->fd, &sockset)) {
            /* socket time out, close it */
            break;
        }

        /* receive data */
        rcn = ssl_conn_rx(conn, buf_in + buf_in_off, LARGEBUF - buf_in_off - 1);

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
                smtp_handlecode(500, conn);

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
                if (smtp_handlecode(smtp_parsel(lns, &stage, mail), conn))
                        goto disconnect;
                if (stage == MAIL && srv == NULL)
                    srv = _m_strdup(mail->froms_v);
            } else { /* processing message data */
                /* check for end of data */
                if (!strcmp(lns, ".")) {
                    /* print out this mail's info (aka `deliver` it) */
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
        memmove(buf_in, lns, LARGEBUF - (lns - buf_in));
        buf_in_off -= (lns - buf_in);
    }
disconnect:
    if (mail != NULL) {
        /* make sure the email was filled */
        if (mail->data_c > 0) {
            (mail->extra)->origin_ip = &addr;
            mail_serialize(mail, server_sf);
        }

        /* Clean up */
        mail_destroy(mail);
    }

    /* close socket */
    ssl_conn_close(conn);
    free(arg);
    free(srv);
    pthread_exit(NULL);
}

void handle_sigint(int sig) {
  prgrm_r = 0;
}

void print_usage(const char *bin, const char *msg) {
    fprintf(stderr, "mail version `"MAILVER"`\n");
    if (msg) fprintf(stderr, "Error: %s\n", msg);
    fprintf(stderr, "Usage: %s [-p port] [-s] [-m mbox directory] [-k /path/to/key] [-c /path/to/cert] <host>\n", bin);
    fprintf(stderr, "\t-p\tthe port to listen on (defaults to 25)\n");
    fprintf(stderr, "\t-s\toutput all mails to STDOUT\n");
    fprintf(stderr, "\t-m\toutput all mails, in MBOX format, to mbox directory\n");
    fprintf(stderr, "\t-k\tpath to server private key\n");
    fprintf(stderr, "\t-c\tpath to server certificate\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\thost:\tserver hostname or IP (e.g., `example.com` or `127.0.0.1`)\n");
    fprintf(stderr, "\t\tMails that are not at the hostnames will be rejected.\n");
    fprintf(stderr, "\t\tTo allow mails to multiple servers, simply specify more server hosts\n");
    fprintf(stderr, "\t\tTo allow mails to any server, do not specify a host.\n");
    fprintf(stderr, "\t\tFirst hostname given will be used to reply, unless none is given (defaults to example.com).\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\tNote: if key and certificate are not both given, the server will not support SSL.\n");
}

char *arr2str(char *const *astr, size_t alen) {
    int i = 0;
    size_t buf_len = 8, buf_used = 0, str_len;
    char *buf, *tmp;
    const char *str;
    buf = malloc(sizeof(char) * buf_len);
    if (!buf) return NULL;

    /* loop through all of the elements of the array and join them with a comma */
    for (i = 0; i < alen; i++) {
        str = astr[i];
        str_len = strlen(str) + 1;

        /* realloc until sufficient memory */
        while (str_len+buf_used > buf_len) {
            buf_len *= 2;
            tmp = realloc(buf, sizeof(char) * buf_len);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
        }

        /* copy over data */
        memcpy(buf+buf_used, str, str_len-1);
        buf_used += str_len;
        /* add comma delimiter */
        buf[buf_used-1] = ',';
    }

    /* check to prevent memory errors */
    if (buf_used == 0) buf_used = 1;
    buf[buf_used-1] = '\0';
    return buf;
}

const char *sf_strs[6] = {"NONE", "STDOUT", "ERR", "BINARY", "MAILBOX", "BOTH"};

int main(int argc, char **argv) {
    /* vars */
    pthread_attr_t attr;
    pthread_t thread;
    struct connection *fd_c;
    int fd_s, port = 25, opt, enable_ssl = 0;
    const char *ssl_key = NULL, *ssl_cert = NULL;
    struct sigaction act;

    /* initial setups */
    server_sf = NONE;

    /* stuff */
    memset(&act, 0, sizeof(struct sigaction));
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER;
    act.sa_handler = handle_sigint;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGINT, &act, NULL);

    /* parse arguments */
    while ((opt = getopt(argc, argv, "p:sm:k:c:")) != -1) {
        switch (opt) {
        case 'p':
            /* port */
            port = atoi(optarg);
            if (port < 0 || port > 65535) {
                print_usage(argv[0], "invalid port specified");
                return 53;
            }
            break;
        case 's':
            /* stdout */
            if (server_sf == MAILBOX)
                server_sf = BOTH;
            else
                server_sf = STDOUT;
            break;
        case 'm':
            /* maildir */
            if (chdir(optarg)) {
                /* failed to cd to directory */
                print_usage(argv[0], "invalid output directory for mail");
                return 63;
            }
            if (server_sf == STDOUT)
                server_sf = BOTH;
            else
                server_sf = MAILBOX;
            break;
        case 'k':
            if (ssl_key) {
                print_usage(argv[0], "only 1 private key allowed");
                return 17;
            }
            ssl_key = optarg;
            enable_ssl++;
            break;
        case 'c':
            if (ssl_cert) {
                print_usage(argv[0], "only 1 certificate allowed");
                return 19;
            }
            ssl_cert = optarg;
            enable_ssl++;
            break;
        default:
            /* unknown flag */
            print_usage(argv[0], "unknown flag");
            return 34;
        }
    }

    /* cry about bad SSL */
    if (enable_ssl == 1) {
        print_usage(argv[0], "missing required parameter for SSL support");
        return 121;
    } else if (enable_ssl) {
        enable_ssl = 1;
    }

    /* build correct hostnames string */
    server_hostname = arr2str(&argv[optind], argc-optind);

    fprintf(stderr, INFO" %s starting!\n", MAILVER);
    fprintf(stderr, INFO" Accepting mails to `%s`\n", server_hostname);
    fprintf(stderr, INFO" Listening on port %d\n", port);
    if (enable_ssl) fprintf(stderr, INFO" SSL via STARTTLS supported\n");
    fprintf(stderr, INFO" Output format: %s\n", sf_strs[server_sf]);

    /* initial socket setup and stuff */
    server_initsocket(&fd_s);
    if (fd_s < 0) return -1;

    if (smtp_gengreeting() < 0) {
        fprintf(stderr, ERR" System appears to be Out of Memory!\n");
        return -17;
    }

    /* initialize SSL */
    if (enable_ssl) {
        if (ssl_global_init(ssl_cert, ssl_key)) {
            fprintf(stderr, ERR" Failed to configure SSL!\n");
            return -12;
        }
    }

    /* bind port */
    if (server_bindport(fd_s, port) < 0) {
        fprintf(stderr, ERR" Could not bind to the port %d!\n", port);
        return -2;
    }

    /* pthread config */
    if (pthread_attr_init(&attr) != 0) {
        fprintf(stderr, ERR" Failed to configure threads!\n");
        return -3;
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        fprintf(stderr, ERR" Failed to configure threads!\n");
        return -4;
    }

    /* loop forever! */
    while (prgrm_r) {
        int fd_ctmp;
        /* accept() it */
        if ((fd_ctmp = accept(fd_s, NULL, NULL)) < 0) {
            fprintf(stderr, ERR" Failed to accept() a connection!\n");
            break;
        } else {
            /* allocate some memory for a new client session */
            fd_c = malloc(sizeof(struct connection)+(enable_ssl?sizeof(struct ssl_ctx):0));

            /* Make sure it allocated correctly */
            if (fd_c == NULL) {
                /* just awkwardly close the connection if OOM */
                close(fd_ctmp);
                continue;
            }

            /* copy over value */
            fd_c->fd = fd_ctmp;
            /* zero value before using it */
            if (enable_ssl) memset(fd_c + 1, 0, sizeof(struct ssl_ctx));
            fd_c->ssl = enable_ssl?(struct ssl_ctx *)(fd_c + 1):NULL;

            /* accepted a new connection, create a thread! */
            pthread_create(&thread, &attr, &server_child, fd_c);
        }
    }

    /* cleanup */
    ssl_global_deinit();
    free(server_hostname);
    free(server_greeting);

    return 0;
}
