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
    SPF_request_t *spf_request = NULL;
    SPF_response_t *spf_response = NULL;

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

    /* check SPF */
    if (self->sconf->spf_server) {
        spf_request = SPF_request_new(self->sconf->spf_server);

        /* set the right ip */
        if (mail->extra.origin_ip.ss_family == AF_INET) {
            if (SPF_request_set_ipv4(spf_request,
                                    ((struct sockaddr_in *)&(mail->extra.origin_ip))->sin_addr))
                goto spf_fail;
        } else if (!strncmp(ip, "::ffff:", 7)) {
            if (SPF_request_set_ipv4_str(spf_request, ip+7))
                goto spf_fail;
        } else {
            if (SPF_request_set_ipv6(spf_request,
                                    ((struct sockaddr_in6 *)&(mail->extra.origin_ip))->sin6_addr))
                goto spf_fail;
        }

        /* set the right domain */
        if (SPF_request_set_env_from(spf_request, strrchr(mail->from_v, '@') + 1))
            goto spf_fail;

        /* send the query */
        SPF_request_query_mailfrom(spf_request, &spf_response);

        /* check the results */
        if (SPF_response_result(spf_response) != SPF_RESULT_PASS) {
            goto spf_fail;
        }
    }

    /* Display log */
    logger_log(INFO, "ACCEPTED mail from %s (%s) %s to %s",
                strncmp(hst, "::ffff:", 7)?hst:&hst[7],
                strncmp(ip, "::ffff:", 7)?ip:&ip[7],
                (mail->extra).using_ssl?"(secured using TLS)":"(not secure)",
                mail->to_v
                );

    /* call serialize function to file */
    mail_serialize_file(mail);

    goto cleanup_mail;

spf_fail:
    /* SPF verification failed, so skip delivering the mail */
    logger_log(INFO, "REJECTED mail from %s (%s) %s to %s due to %s",
                strncmp(hst, "::ffff:", 7)?hst:&hst[7],
                strncmp(ip, "::ffff:", 7)?ip:&ip[7],
                (mail->extra).using_ssl?"(secured using TLS)":"(not secure)",
                mail->to_v,
                spf_response?SPF_strresult(SPF_response_result(spf_response)):"SPF verification failure"
                );

cleanup_mail:
    /* cleanup when done */
    mail_destroy(mail);

    if (spf_response) {
        SPF_response_free(spf_response);
        spf_response = NULL;
    }
    if (spf_request) {
        SPF_request_free(spf_request);
        spf_request = NULL;
    }

next:
    if (!running) goto end;
    else goto start;

end:
    return 0;
}
