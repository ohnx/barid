#include "server.h"

void server_initsocket(struct server *state) {
    /* make a socket */
    if ((state->socket = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, ERR"Could not create a socket!\n");
        return;
    }

    /* be nice.. set SO_REUSEADDR */
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,sizeof(on)) < 0) {
        fprintf(stderr, WARN"Could not set SO_REUSEADDR on socket!\n");
    }
}

int server_bindport(struct server *state, int port) {
    struct sockaddr_in6 saddr;

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port   = htons(port);
    saddr.sin6_addr   = in6addr_any;

    return bind(state->socket, (struct sockaddr *)&saddr, sizeof(saddr));
}

void *server_child(void *arg) {
    /* vars */
    struct session *client_sess;
    char buf_in[LARGEBUF], buf_out[SMALLBUF];
    struct timeval timeout;
    flag in_data = 0;

    /* initial values */
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;

    /* extract the required data from args */
    client_sess = (struct session *)arg;

    /* TODO: Not generate the greeting every. single. thread. */
    /* initial greeting */
    sprintf(buf_out, "220 %s %s\r\n", (client_sess->parent)->domain, MAILVER);
    send(client_sess->fd, buf_out, strlen(buf_out), 0);

    /* loops! yay! */
    while (1) {
        /* vars */
        fd_set sockset;
        int buf_remain

        /* we use select() here to have a timeout */
        FD_ZERO(&sockset);
        FD_SET(client_sess->fd, &sockset);

        select(client_sess->fd + 1, &sockset, NULL, NULL, &timeout);

        /* check for timeout */
        if (!FD_ISSET(client_sess->fd, &sockset)) {
            /* socket time out */
            break;
        }

        /* while there is still data left to parse */
            /* receive data into buffer */
            /* parse the data line-by-line into the mail struct */
    }
    free(client_sess);
}

int main() {
    /* vars */
    struct server state;
    int port;
    struct session *client_sess;

    /* user config */
    port = 25;

    /* initial socket setup and stuff */
    server_initsocket(state);
    if (state->socket < 0) return -1;

    if (server_bindport(state, port) < 0) {
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
            client_sess->parent = state;
            client_sess->data = NULL;
        }

        /* wait for a connection to our server... */
        if ((client_sess->fd = accept(state->socket, NULL, NULL)) < 0) {
            fprintf(stderr, ERR"Failed to accept() a connection!");
            continue;
        } else {
            /* accepted a new connection, create a thread! */
            pthread_create(&(state.thread), NULL, &server_child, client_sess);

            /* require a new allocation for the next thread */
            client_sess = NULL;
        }
    }
}
