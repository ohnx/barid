/* this file deals with setting up mail information */

/* calloc(), free(), etc. */
#include <stdlib.h>
/* memcpy(), etc. */
#include <string.h>
/* getpeername() */
#include <sys/socket.h>

/* struct mail, etc. */
#include "common.h"
/* MAIL_ERROR_OOM, etc. */
#include "mail.h"

char *allowed_hosts;

void mail_set_allowed(struct barid_conf *conf) {
    allowed_hosts = conf->domains;
}

struct mail *mail_new(const char *froms) {
    struct mail *email;
    int i;

    email = calloc(1, sizeof(struct mail));
    if (email == NULL) return NULL;

    /* initial allocations for expandable buffers */
    email->extra.to_total_len = 128;
    email->to_v = calloc(email->extra.to_total_len, 1);
    if (email->to_v == NULL) {
        free(email);
        return NULL;
    }

    email->extra.data_total_len = 256;
    email->data_v = calloc(email->extra.data_total_len, 1);
    if (email->data_v == NULL) {
        free(email->to_v);
        free(email);
        return NULL;
    }

    /* from server */
    if (froms) {
        i = strlen(froms);
        email->froms_c = i + 1;
        email->froms_v = malloc(i + 1);
        if (email->from_v == NULL) {
            free(email->to_v);
            free(email->data_v);
            free(email);
            return NULL;
        }

        memcpy(email->froms_v, froms, email->froms_c);
    }

    return email;
}

int mail_reset(struct mail *email) {
    /* clear from */
    free(email->from_v);
    email->from_v = NULL;
    email->from_c = 0;
    /* clear to */
    email->to_v[0] = '\0';
    email->to_c = 0;
    /* clear data */
    email->data_v[0] = '\0';
    email->data_c = 0;
    return 0;
}

int mail_setattr(struct mail *email, enum mail_attr attr, const char *data) {
    int data_len, i, t;
    if (data)
        data_len = strlen(data);
    t = 0;

    switch(attr) {
    case FROMS:
        /* ensure there is a server (if not, return error) */
        if (data_len == 0) return MAIL_ERROR_PARSE;

        /* copy the data */
        if (email->froms_v) free(email->froms_v);
        email->froms_c = data_len + 1;
        email->froms_v = malloc(email->froms_c);

        /* successful allocation check */
        if (email->froms_v == NULL) return MAIL_ERROR_OOM;

        /* copy the data */
        memcpy(email->froms_v, data, email->froms_c);

        /* no error */
        return MAIL_ERROR_NONE;
    case FROM:
        /* ensure brackets around email (if not, return error) */
        if (!(*data == '<' && data[data_len-1] == '>')) return MAIL_ERROR_PARSE;
        

        /* ensure exactly at least one @ symbol present */
        for (i = 0; i < data_len; i++) {
            if (data[i] == '@') t++;
            /* Newlines are not allowed in the mail FROM */
            if (data[i] == '\n') return MAIL_ERROR_INVALIDEMAIL;
        }
        if (t < 1) return MAIL_ERROR_PARSE;

        /* for a FROM email, that's all the checking we will do. */
        email->from_c = data_len - 1; /* subtract the '<' and '>', but add '\0' */
        email->from_v = malloc(email->from_c);

        /* successful allocation check */
        if (email->from_v == NULL) return MAIL_ERROR_OOM;

        /* copy the data, omitting the start '<' and end '\0' */
        memcpy(email->from_v, data + 1, email->from_c);

        /* replace end '>' with '\0' */
        email->from_v[email->from_c - 1] = 0;

        /* no error */
        return MAIL_ERROR_NONE;
    case SSL_USED:
        (email->extra).using_ssl = 1;

        return MAIL_ERROR_NONE;
    case REMOTE_ADDR:
        if (!data) return MAIL_ERROR_PROGRAM;
        /* getpeername() fetches the peer's ip address */
        t = sizeof(email->extra.origin_ip);
        if (getpeername(*((int *)(data)), (struct sockaddr *)&(email->extra.origin_ip), (unsigned int *)&t) < 0)
            return MAIL_ERROR_MISC;
        else
            return MAIL_ERROR_NONE;
    default:
        /* should be calling mail_addattr() */
        return MAIL_ERROR_PROGRAM;
    }
}

static int mail_checkinlist(const char *str, char *list, size_t len) {
    char *ptr = list, *start = list;

    /* loop through hostnames in server_hostname */
    do {
        if (*ptr == ' ') {
            /* found end of hostname */
            *ptr = '\0';

            /* return if equal */
            if (!strncmp(str, start, len)) {
                *ptr = ' ';
                return 1;
            }

            /* not equal, keep searching */
            start = ptr + 1;
            *ptr = ' ';
        }
    } while (*(ptr++) != '\0');

    /* need to backtrack if we went over the string end */
    if (*(ptr-1) == '\0') ptr--;

    /* last chance, maybe the last hostname is the right one */
    if (!strncmp(str, start, len)) return 1;

    return 0;
}

int mail_addattr(struct mail *email, enum mail_attr attr, const char *data) {
    int data_len, i;
    const char *atpos;
    data_len = strlen(data) + 1;
    atpos = NULL;

    switch(attr) {
    case TO:
        /* ensure brackets around email (if not, return error) */
        if (!(*data == '<' && data[data_len-2] == '>')) return MAIL_ERROR_PARSE;

        /* ensure valid TO address */
        /* starting with a dot is not allowed */
        if (data[1] == '.') return MAIL_ERROR_INVALIDEMAIL;
        for (i = 0; i < data_len-1; i++) {
            if (data[i] == '@') atpos = data + i;
            /* forward slashes not allowed */
            if (data[i] == '/') return MAIL_ERROR_INVALIDEMAIL;
        }
        if (atpos == NULL) return MAIL_ERROR_PARSE;

        /* temporarily remove the trailing '>' */
        /* ensure valid domain (*server_hostname = '*' = wildcard listen) */
        if (*allowed_hosts != '*' && !mail_checkinlist(atpos+1, allowed_hosts, data_len-(atpos-data)-3)) {
            return MAIL_ERROR_USRNOTLOCAL;
        }

        /* check there's enough room for this address */
        while (((email->extra).to_total_len) < (email->to_c + data_len)) {
            void *x;
            /* reallocate memory */
            (email->extra).to_total_len *= 2;

            /* ensure max bounds for memory allocation */
            if (((email->extra).to_total_len) > MAIL_MAX_TO_C)
                return MAIL_ERROR_RCPTMAX;

            /* reallocation + error checking */
            x = realloc(email->to_v, (email->extra).to_total_len);
            if (!x) return MAIL_ERROR_OOM;
            email->to_v = (char *)x;
        }

        /* copy the data, omitting the start '<' and end '\0' */
        memcpy(email->to_v + email->to_c, data + 1, data_len - 2);

        /* replace end '>' with '\0' */
        email->to_v[email->to_c + data_len - 3] = 0;

        /* update bytes used */
        email->to_c += data_len - 2;

        /* no error */
        return MAIL_ERROR_NONE;
    default:
        /* should be calling mail_setattr() */
        return MAIL_ERROR_PROGRAM;
    }
}

int mail_appenddata(struct mail *email, const char *data) {
    int data_len;
    char appB;

    /* Replace .. with single . */
    if (*data == '.') data++;

    /* get data length */
    data_len = strlen(data) + 1;
    
    /* Need to add a '>' to 'From ' (there is corruption here, oops) */
    appB = !strncmp(data, "From ", 5);
    if (appB) data_len++;

    /* check there's enough room for this line of data (if not, realloc) */
    while (((email->extra).data_total_len) < (email->data_c + data_len)) {
        void *x;
        /* reallocate memory */
        (email->extra).data_total_len *= 2;

        /* ensure max bounds for memory allocation */
        if (((email->extra).data_total_len) > MAIL_MAX_DATA_C)
            return MAIL_ERROR_DATAMAX;

        /* reallocation + error checking */
        x = realloc(email->data_v, (email->extra).data_total_len);
        if (!x) return MAIL_ERROR_OOM;
        email->data_v = (char *)x;
    }

    /* copy data over */
    if (!appB)
        memcpy(email->data_v + email->data_c, data, data_len);
    else {
        /* Need to add a '>' to 'From ' (there is corruption here, oops) */
        memcpy(email->data_v + email->data_c + 1, data, data_len - 1);
        *(email->data_v + email->data_c) = '>';
    }

    /* update bytes used */
    email->data_c += data_len - 1; /* we want the next copy to overwrite NULL */

    /* no error */
    return MAIL_ERROR_NONE;
}

void mail_destroy(struct mail *email) {
    free(email->froms_v);
    free(email->from_v);
    free(email->to_v);
    free(email->data_v);
    free(email);
}
