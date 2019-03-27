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

int smtp_handlecode(struct client *client, int code) {
    /* the ternary here is to detect if send() had any errors */
    switch (code) {
    /* 200-family */
    case 220:
        return net_tx(client, server_greeting, server_greeting_len) > 0 ? 0 : 1;
    case 8220: /* STARTTLS is going to happen */
        net_tx(client, "220 LET'S STARTTLS\r\n", 20);
        return ssl_client_start(client);
    case 221:
        return net_tx(client, "221 BYE\r\n", 9) + 2; /* always close */
    case 250:
        return net_tx(client, "250 OK\r\n", 8) > 0 ? 0 : 1; /* close on error */
    case 8250: /* returned by EHLO 250 OK */
        net_tx(client, "250-barid mail server\r\n", 23);
        if (enable_ssl) net_tx(client, "250-STARTTLS\r\n", 14);
        return net_tx(client, "250 RSET\r\n", 10) > 0 ? 0 : 1;
    /* 300-family */
    case 354:
        return net_tx(client, "354 SEND DATA PLZ!\r\n", 20) > 0 ? 0 : 1;
    /* 400-family server-caused error */
    case 422:
        return net_tx(client, "422 MAILBOX FULL!!\r\n", 20) > 0 ? 0 : 1;
    case 450:
        return net_tx(client, "450 BAD MAILBOX :(\r\n", 20) > 0 ? 0 : 1;
    case 451:
        return net_tx(client, "451 INTERNAL ERROR\r\n", 20) > 0 ? 0 : 1;
    case 452:
        return net_tx(client, "452 TOO MANY RCPTS\r\n", 20) > 0 ? 0 : 1;
    /* 500-family client-caused error */
    case 500:
        return net_tx(client, "500 LINE TOO LONG!\r\n", 20) > 0 ? 0 : 1;
    case 501:
        return net_tx(client, "501 ARGUMENT ERROR\r\n", 20) > 0 ? 0 : 1;
    case 502:
        return net_tx(client, "502 NOTIMPLEMENTED\r\n", 20) > 0 ? 0 : 1;
    case 503:
        return net_tx(client, "503 WRONG SEQUENCE\r\n", 20) > 0 ? 0 : 1;
    case 522:
        return net_tx(client, "522 TOO MUCH DATA!\r\n", 20) > 0 ? 0 : 1;
    case 550:
        return net_tx(client, "550 USER NOT LOCAL\r\n", 20) > 0 ? 0 : 1;
    /* Unknown??!? */
    default:
        return 0;
    }
}
