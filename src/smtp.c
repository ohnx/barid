/* this file deals with all of the SMTP protocol stuff */
#include "smtp.h"

int smtp_gengreeting() {
    char *ptr = server_hostname, *smtp_host = server_hostname, og;

    /* get first hostname in server_hostname */
    do {
        if (*ptr == ',') break;
    } while (*(ptr++) != '\0');
    /* need to backtrack if we went over the string end */
    if (*(ptr-1) == '\0') ptr--;

    /* check if we are doing a wildcard or if it's a specific hostname*/
    og = *ptr;
    if (ptr == server_hostname) smtp_host = "example.com";
    else *ptr = '\0';

    /* 7 chars in `220  \r\n`, plus 1 for null = 8 */
    server_greeting_len = 8 + strlen(smtp_host) + strlen(MAILVER);

    /* allocate memory */
    server_greeting = calloc(server_greeting_len, sizeof(char));

    /* OOM */
    if (server_greeting == NULL) {
        *ptr = og;
        return -1;
    }

    /* generate server greeting */
    sprintf(server_greeting, "220 %s %s\r\n", smtp_host, MAILVER);

    /* WE DO NOT ACTUALLY WANT TO SEND THE NULL!!! REMOVE IT FROM COUNT */
    server_greeting_len--;

    /* no error */
    *ptr = og;
    return 0;
}

int smtp_handlecode(int code, struct connection *conn) {
    /* the ternary here is to detect if send() had any errors */
    switch (code) {
    /* 200-family */
    case 220:
        return ssl_conn_tx(conn, server_greeting, server_greeting_len) > 0 ? 0 : 1;
#ifndef __FUZZ
    case 8220: /* STARTTLS is going to happen */
        ssl_conn_tx(conn, "220 LET'S STARTTLS\r\n", 20);
        return ssl_conn_start(conn);
#endif
    case 221:
        return ssl_conn_tx(conn, "221 BYE\r\n", 9) + 2; /* always close */
    case 250:
        return ssl_conn_tx(conn, "250 OK\r\n", 8) > 0 ? 0 : 1; /* close on error */
    case 8250: /* returned by EHLO 250 OK */
        ssl_conn_tx(conn, "250-barid mail server\r\n", 23);
        if (enable_ssl) ssl_conn_tx(conn, "250-STARTTLS\r\n", 14);
        return ssl_conn_tx(conn, "250 RSET\r\n", 10) > 0 ? 0 : 1;
    /* 300-family */
    case 354:
        return ssl_conn_tx(conn, "354 SEND DATA PLZ!\r\n", 20) > 0 ? 0 : 1;
    /* 400-family server-caused error */
    case 422:
        return ssl_conn_tx(conn, "422 MAILBOX FULL!!\r\n", 20) > 0 ? 0 : 1;
    case 450:
        return ssl_conn_tx(conn, "450 BAD MAILBOX :(\r\n", 20) > 0 ? 0 : 1;
    case 451:
        return ssl_conn_tx(conn, "451 INTERNAL ERROR\r\n", 20) > 0 ? 0 : 1;
    case 452:
        return ssl_conn_tx(conn, "452 TOO MANY RCPTS\r\n", 20) > 0 ? 0 : 1;
    /* 500-family client-caused error */
    case 500:
        return ssl_conn_tx(conn, "500 LINE TOO LONG!\r\n", 20) > 0 ? 0 : 1;
    case 501:
        return ssl_conn_tx(conn, "501 ARGUMENT ERROR\r\n", 20) > 0 ? 0 : 1;
    case 502:
        return ssl_conn_tx(conn, "502 NOTIMPLEMENTED\r\n", 20) > 0 ? 0 : 1;
    case 503:
        return ssl_conn_tx(conn, "503 WRONG SEQUENCE\r\n", 20) > 0 ? 0 : 1;
    case 522:
        return ssl_conn_tx(conn, "522 TOO MUCH DATA!\r\n", 20) > 0 ? 0 : 1;
    case 550:
        return ssl_conn_tx(conn, "550 USER NOT LOCAL\r\n", 20) > 0 ? 0 : 1;
    /* Unknown??!? */
    default:
        return 0;
    }
}

int smtp_parsel(char *line, enum server_stage *stage, struct mail *mail) {
    int i, line_len;

    /* convert verb to upper case if it's in lower case */
    for (i = 0; i < 4; i++) {
        if (line[i] >= 'a' && line[i] <= 'z') {
            line[i] -= 32;
        }
    }

    /* Get line length */
    line_len = strlen(line);

    /* check simple verbs */
    if (!strncmp(line, "NOOP", 4)) { /* do nothing */
        return 250;
    } else if (!strncmp(line, "RSET", 4)) { /* reset connection */
        mail_reset(mail);
        *stage = MAIL;
        return 250;
    } else if (!strncmp(line, "QUIT", 4)) { /* bye! */
        return 221;
    }

    /* Check that mail is not null */
    if (!mail) { /* OOM */
        return 451;
    }

    /* more complex logic verbs */
    if (!strncmp(line, "HELO", 4) || !strncmp(line, "EHLO", 4)) { /* greeting */
        /* ensure correct order */
        if (!(*stage == HELO)) {
            return 503;
        }

        /* ensure valid HELO/EHLO message */
        if (line_len < 5) {
            return 501;
        }

        /* parse out sending server and check return status */
        i = mail_setattr(mail, FROMS, line + 5);
        if (i == MAIL_ERROR_PARSE) { /* error parsing */
            return 501;
        } else if (i == MAIL_ERROR_OOM) { /* server out of memory */
            return 451;
        } else if (i == MAIL_ERROR_PROGRAM) { /* program error */
            fprintf(stderr, ERR"The programmer has made an error... ");
            fprintf(stderr, "(%s:%d) %s\n", __FILE__, __LINE__, MAILVER);
            fprintf(stderr, ERR"Please report this!\n");
            exit(-112);
        }

        /* update stage */
        *stage = MAIL;

        /* check request - EHLO will have more returns and we create a custom SMTP code for it */
        if (*line == 'E') return 8250;
    } else if (!strncmp(line, "MAIL", 4)) { /* got mail from ... */
        /* ensure correct order */
        if (!(*stage == MAIL)) return 503;

        /* ensure valid MAIL FROM: message */
        if (line_len < 10) return 501;

        /* parse out sending server and check return status */
        i = mail_setattr(mail, FROM, line + 10);
        if (i == MAIL_ERROR_PARSE) { /* error parsing */
            return 501;
        } else if (i == MAIL_ERROR_OOM) { /* server out of memory */
            return 451;
        } else if (i == MAIL_ERROR_PROGRAM) { /* program error */
            fprintf(stderr, ERR"The programmer has made an error... ");
            fprintf(stderr, "(%s:%d) %s\n", __FILE__, __LINE__, MAILVER);
            fprintf(stderr, ERR"Please report this!\n");
            exit(-134);
        }

        /* update stage */
        *stage = RCPT;
    } else if (!strncmp(line, "RCPT", 4)) { /* mail addressed to */
        /* ensure correct order */
        if (!(*stage == RCPT) && !(*stage == DATA)) return 503;

        /* ensure valid RCPT TO: message */
        if (line_len < 8) return 501;
        
        /* parse out sending server and check return status */
        i = mail_addattr(mail, TO, line + 8);
        if (i == MAIL_ERROR_PARSE) { /* error parsing */
            return 501;
        } else if (i == MAIL_ERROR_OOM) { /* server out of memory */
            return 451;
        } else if (i == MAIL_ERROR_INVALIDEMAIL) { /* bad dest address */
            return 450; /* no forward slashes in an email is actually
                         * a restriction set by `mail`, not by rfc's. */
        } else if (i == MAIL_ERROR_RCPTMAX) { /* too many recipients */
            return 452;
        } else if (i == MAIL_ERROR_USRNOTLOCAL) {
            return 550;
        } else if (i == MAIL_ERROR_PROGRAM) { /* program error */
            fprintf(stderr, ERR"The programmer has made an error... ");
            fprintf(stderr, "(%s:%d) %s\n", __FILE__, __LINE__, MAILVER);
            fprintf(stderr, ERR"Please report this!\n");
            exit(-161);
        }

        /* update stage */
        *stage = DATA;
    } else if (!strncmp(line, "DATA", 4)) { /* message data */
        /* ensure correct order */
        if (!(*stage == DATA)) return 503;

        /* update stage */
        *stage = END_DATA;

        /* send CONTINUE message */
        return 354;
    } else if (!strncmp(line, "STARTTLS", 8)) {
        if (!(*stage == MAIL)) return 503;
        if (!enable_ssl) return 502;
        *stage = HELO;
        /* let the mail portion know that SSL was used */
        mail_setattr(mail, SSL_USED, NULL);

        /* custom code */
        return 8220;
    } else {
        return 502;
    }

    /* fallthrough to OK */
    return 250;
}
