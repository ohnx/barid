#include "mail.h"

int mail_serialize(struct mail *email, enum mail_sf format) {
    int r;
    switch (format) {
    case MAILBOX:
    case BOTH:
        r = mail_serialize_file(email);
        if (r) return r;
        if (format == MAILBOX) return r;
    default:
        r = mail_serialize_stdout(email);
        return r;
    }
}

int mail_serialize_stdout(struct mail *email) {
    int i = 0;
    char ip[46];
    char hst[NI_MAXHOST];
    struct sockaddr_storage *a;

    /* ip info */
    a = (email->extra)->origin_ip;
    if (a->ss_family == AF_INET6)
        inet_ntop(a->ss_family, &((struct sockaddr_in6 *)a)->sin6_addr, ip, 46);
    else
        inet_ntop(a->ss_family, &((struct sockaddr_in *)a)->sin_addr, ip, 46);
    getnameinfo((struct sockaddr *)a, sizeof(*a), hst, sizeof(hst), NULL, 0, 0);

    /* print out info */
    printf("----------------------\n");
    printf("new email from `%s` ", strncmp(hst, "::ffff:", 7)?hst:&hst[7]);
    printf("(%s)\n", strncmp(ip, "::ffff:", 7)?ip:&ip[7]);
    printf("reported sender server: `%s`\n", email->froms_v);
    printf("reported sender email: `%s`\n", email->from_v);
    printf("reported recipients:\n");
    for (i = 0; i < email->to_c; i++) {
        printf("\t`%s`\n", email->to_v + i);
        i += strlen(email->to_v + i);
    }
    printf("given data: ```\n%s```\n", email->data_v);
    return 0;
}

void ms_chmod(const char *out_name) {
    uid_t          uid;
    gid_t          gid;
    struct passwd *pwd;
    struct group  *grp;

    /* get user id */
    pwd = getpwnam(out_name);
    if (pwd == NULL) {
        return;
    }
    uid = pwd->pw_uid;

    /* get group id */
    grp = getgrnam(out_name);
    if (grp == NULL) {
        return;
    }
    gid = grp->gr_gid;

    /* do the actual chown */
    if (chown(out_name, uid, gid) == -1) {
        return;
    }
}

int mail_serialize_file(struct mail *email) {
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

        /* chown the file */
        ms_chmod(fo);

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
