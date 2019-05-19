/* this file does most of disk i/o */
/* free() */
#include <stdlib.h>
/* write(), read() */
#include <unistd.h>
/* strlen() */
#include <string.h>
/* inet_ntop() */
#include <arpa/inet.h>
/* getnameinfo() */
#include <netdb.h>

/* running flag */
#include "common.h"
/* struct serworker */
#include "serworker.h"
/* logger_log() */
#include "logger.h"
/* mail_destroy() */
#include "mail.h"

int serworker_deliver(int fd, struct mail *mail) {
    return sizeof(mail) - write(fd, &mail, sizeof(mail));
}

#ifndef USE_PTHREADS
int serworker_loop(void *z) {
#else
void *serworker_loop(void *z) {
#endif
    struct serworker *self = (struct serworker *)z;
    struct mail *mail;
    unsigned int len;
    char ip[46], hst[NI_MAXHOST];

start:
    /* read in the pointer to the mail object to serialize */
    len = read(self->pfd, &mail, sizeof(mail));

    /* data read was not a pointer */
    if (len != sizeof(mail)) goto next;

    /* ip info */
    if (mail->extra.origin_ip.ss_family == AF_INET6)
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&(mail->extra.origin_ip))->sin6_addr), ip, 46);
    else
        inet_ntop(AF_INET, &(((struct sockaddr_in *)&(mail->extra.origin_ip))->sin_addr), ip, 46);
    getnameinfo((struct sockaddr *)(&(mail->extra.origin_ip)), sizeof(mail->extra.origin_ip), hst, sizeof(hst), NULL, 0, 0);

    /* Display log */
    logger_log(INFO, "Mail from %s (%s) %s",
                strncmp(hst, "::ffff:", 7)?hst:&hst[7],
                strncmp(ip, "::ffff:", 7)?ip:&ip[7],
                (mail->extra).using_ssl?"(secured using TLS)":"(not secure)"
                );

    /* call serialize function to file */
    mail_serialize_file(mail);

    /* cleanup when done */
    mail_destroy(mail);

next:
    if (!running) goto end;
    else goto start;

end:
    return 0;
}
