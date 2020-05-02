/* this file deals with setting up mail information */

/* calloc(), free(), etc. */
#include <stdlib.h>
/* memcpy(), etc. */
#include <string.h>
/* getpeername() */
#include <sys/socket.h>
/* time(), etc. */
#include <time.h>

/* struct mail, etc. */
#include "common.h"
/* MAIL_ERROR_OOM, etc. */
#include "mail.h"

int mail_serialize_mbox(struct mail *email, const char *account) {
    char *from_fix;
    char timebuf[26];
    int i = 0;
    time_t timep;
    FILE *fp;

    /* get time as string */
    time(&timep);
    ctime_r(&timep, timebuf);

    /* Replace ' ' with '-' in mbox format */
    from_fix = strdup(email->from_v);
    for (i = 0; i < email->from_c; i++)
        if (from_fix[i] == ' ')
            from_fix[i] = '-';

    /* open the specified file */
    fp = fopen(account, "a");
    if (fp == NULL) goto error;

    /* get time (timebuf has \n already) */
    if (fprintf(fp, "From %s %s", from_fix, timebuf) < 0) goto error;

    /* print out data */
    if (fprintf(fp, "%s\n", email->data_v) < 0) goto error;

    /* close file + clean up */
    if (fclose(fp) != 0) goto error;

    /* all good! */
    free(from_fix);
    return 0;

error:
    /* failed to open file, return error */
    free(from_fix);
    return -1;
}

int mkdirp(const char *dir) {
    (void)dir;
    return 0;
}

int mail_serialize_maildir(struct mail *email, const char *account) {
    (void)email;
    (void)account;
    /* create directories */

    /* come up with unique file name */

    /* write to the file */



    return 0;
}
