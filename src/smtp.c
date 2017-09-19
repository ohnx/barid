/* There are TODO's in this file. */
#include "smtp.h"

int smtp_handlecode(int code, int fd) {
    switch (code) {
    /* 200-family */
    case 221:
        send(fd, "221 BYE\r\n", 9, 0);
        return 1;
    case 250:
        send(fd, "250 OK\r\n", 8, 0);
        return 0;
    /* 300-family */
    case 354:
        send(fd, "354 SEND DATA PLZ!\r\n", 20, 0);
        return 0;
    /* 400-family server-caused error */
    case 450:
        send(fd, "450 BAD MAILBOX :(\r\n", 20, 0);
        return 0;
    case 451:
        send(fd, "451 INTERNAL ERROR\r\n", 20, 0);
        return 0;
    case 452:
        send(fd, "452 TOO MANY RCPTS\r\n", 20, 0);
        return 0;
    /* 500-family client-caused error */
    case 500:
        send(fd, "500 LINE TOO LONG!\r\n", 20, 0);
    case 501:
        send(fd, "501 ARGUMENT ERROR\r\n", 20, 0);
        return 0;
    case 502:
        send(fd, "502 NOTIMPLEMENTED\r\n", 20, 0);
        return 0;
    case 503:
        send(fd, "503 WRONG SEQUENCE\r\n", 20, 0);
        return 0;
    case 522:
        send(fd, "522 TOO MANY DATA!\r\n", 20, 0);
        return 0;
    /* Unknown??!? */
    default:
        return 0;
    }
}

int smtp_parsel(char *line, enum server_stage *stage, struct mail *mail) {
    int i, line_len;

    /* convert to upper case if it's in lower case */
    for (i = 0; i < 4; i++) {
        if (line[i] >= 'a' && line[i] < 'z') {
            line[i] -= 32;
        }
    }

    /* Get line length */
    line_len = strlen(line);

    /* check simple verbs */
    if (!strncmp(line, "NOOP", 4)) { /* do nothing */
        return 250;
    } else if (!strncmp(line, "RSET", 4)) { /* reset connection */
        *stage = HELO;
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
            /* TODO: HANDLE THIS */
        }

        /* update stage */
        *stage = MAIL;
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
            /* TODO: HANDLE THIS */
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
        } else if (i == MAIL_ERROR_PROGRAM) { /* program error */
            /* TODO: HANDLE THIS */
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
    } else {
        return 502;
    }

    /* fallthrough to OK */
    return 250;
}