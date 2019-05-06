/* this file does most of disk i/o */
/* free() */
#include <stdlib.h>
/* write(), read() */
#include <unistd.h>
/* strlen() */
#include <string.h>

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

void *serworker_loop(void *z) {
    struct serworker *self = (struct serworker *)z;
    struct mail *mail;
    unsigned int len;
    int i;

start:
    /* read in the pointer to the mail object to serialize */
    len = read(self->pfd, &mail, sizeof(mail));

    if (len != sizeof(mail)) {
        /*logger_log(WARN, "Failed to deliver a mail!");*/
        goto next;
    }

    /* serialize it */
    logger_log(INFO, "Delivering mail at address %p", mail);

    printf("From: `%s` via `%s`\n", mail->from_v, mail->froms_v);
    printf("To: ");
    for (i = 0; i < mail->to_c; i++) {
        printf("`%s`", mail->to_v + i);
        i += strlen(mail->to_v + i);
        if (i <= mail->to_c) printf(",");
    }
    printf("\n");
    printf("Body: ```\n%s```\n", mail->data_v);

    /* cleanup when done */
    mail_destroy(mail);

next:
    if (!running) goto end;
    else goto start;

end:
    return NULL;
}
