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

int mail_serialize_mbox(struct mail *email) {
    char *at_loc, *fo, *fo_o, *from_fix, *cps;
    char timebuf[26];
    time_t timep;
    int i;
    FILE *fp;

    /* get time as string */
    time(&timep);
    ctime_r(&timep, timebuf);

    /* Replace ' ' with '-' in mbox format */
    from_fix = strdup(email->from_v);
    for (i = 0; i < email->from_c; i++)
        if (from_fix[i] == ' ')
            from_fix[i] = '-';

    /* loop through all addresses to deliver to */
    for (i = 0; i < email->to_c; i++) {
        /* temporarily clear '@' to ignore domain */
        at_loc = strchr(email->to_v + i, '@');
        *at_loc = 0;

        /* allocate memory for file output name */
        fo = fo_o = strdup(email->to_v + i);

        /* ignore all characters after last + sign */
        cps = strchr(fo, '+');
        if (cps) *cps = 0;

        /* check for comments at start and skip past them */
        if (*fo == '(' && (cps = strchr(fo, ')')) != NULL) fo = cps+1;

        /* check for comments at end and null them out */
        if (fo[strlen(fo)] == ')' && (cps = strchr(fo, '(')) != NULL) *cps = 0;

        /* open the specified file */
        fp = fopen(fo, "a");
        if (fp == NULL) goto error;

        /* get time (timebuf has \n already) */
        if (fprintf(fp, "From %s %s", from_fix, timebuf) < 0) goto error;

        /* print out data */
        if (fprintf(fp, "%s\n", email->data_v) < 0) goto error;

        /* close file + clean up */
        if (fclose(fp) != 0) goto error;

        free(fo_o);
        fo_o = NULL;

        /* advance to next one */
        *at_loc = '@';
        i += strlen(email->to_v + i);
        continue;

    error:
        /* failed to open file, return error */
        free(fo_o);
        free(from_fix);
        return -1;
    }

    free(from_fix);
    return 0;
}

int mail_serialize_maildir(struct mail *email) {
    (void)email;
    return 0;
}
