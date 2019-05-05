/* this file deals with all of the SMTP protocol stuff outside of the parsing */
#include "smtp.h"

/* struct client */
#include "common.h"
/* net_tx() */
#include "net.h"

typedef unsigned char *ucp;

int smtp_handlecode(struct client *client, int code) {
    /* the ternary here is to detect if send() had any errors */
    switch (code) {
    /* 200-family */
    case 220: return net_tx(client, (ucp)"220 HELO\r\n", 10) > 0 ? 0 : 1;
    case 8220: /* STARTTLS is going to happen */
        net_tx(client, (ucp)"220 LET'S STARTTLS\r\n", 20);
        return net_sssl(client);
    case 221: return net_tx(client, (ucp)"221 BYE\r\n", 9) + 2; /* always close */
    case 250: return net_tx(client, (ucp)"250 OK\r\n", 8) > 0 ? 0 : 1; /* close on error */
    case 8250: /* returned by EHLO 250 OK */
        net_tx(client, (ucp)"250-barid mail server\r\n", 23);
        /* TODO: add STARTTLS */
        if (0) net_tx(client, (ucp)"250-STARTTLS\r\n", 14);
        return net_tx(client, (ucp)"250 RSET\r\n", 10) > 0 ? 0 : 1;
    /* 300-family */
    case 354: return net_tx(client, (ucp)"354 SEND DATA PLZ!\r\n", 20) > 0 ? 0 : 1;
    /* 400-family server-caused error */
    case 422: return net_tx(client, (ucp)"422 MAILBOX FULL!!\r\n", 20) > 0 ? 0 : 1;
    case 450: return net_tx(client, (ucp)"450 BAD MAILBOX :(\r\n", 20) > 0 ? 0 : 1;
    case 451: return net_tx(client, (ucp)"451 INTERNAL ERROR\r\n", 20) > 0 ? 0 : 1;
    case 452: return net_tx(client, (ucp)"452 TOO MANY RCPTS\r\n", 20) > 0 ? 0 : 1;
    /* 500-family client-caused error */
    case 500: return net_tx(client, (ucp)"500 LINE TOO LONG!\r\n", 20) > 0 ? 0 : 1;
    case 501: return net_tx(client, (ucp)"501 ARGUMENT ERROR\r\n", 20) > 0 ? 0 : 1;
    case 502: return net_tx(client, (ucp)"502 NOTIMPLEMENTED\r\n", 20) > 0 ? 0 : 1;
    case 503: return net_tx(client, (ucp)"503 WRONG SEQUENCE\r\n", 20) > 0 ? 0 : 1;
    case 522: return net_tx(client, (ucp)"522 TOO MUCH DATA!\r\n", 20) > 0 ? 0 : 1;
    case 550: return net_tx(client, (ucp)"550 USER NOT LOCAL\r\n", 20) > 0 ? 0 : 1;
    /* Unknown??!? */
    default: return 0;
    }
}
