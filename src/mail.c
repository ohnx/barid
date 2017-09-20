/* There are TODO's in this file. */
#include "mail.h"
/*https://serverfault.com/tags/mbox/info*/

struct mail *mail_new_internal(int hasExtra) {
    struct mail *email;

    email = calloc(1, sizeof(struct mail));

    /* r/w email */
    if (email == NULL) return NULL;
    if (hasExtra) {
        email->extra = calloc(1, sizeof(struct mail_internal_info));

        /* error on OOM */
        if (email->extra == NULL) {
            free(email);
            return NULL;
        }

        /* initial allocations for expandable buffers */
        (email->extra)->to_total_len = 256;
        email->to_v = calloc(256, 1);
        if (email->to_v == NULL) {
            free(email->extra);
            free(email);
            return NULL;
        }
        (email->extra)->data_total_len = 512;
        email->data_v = calloc(512, 1);
        if (email->data_v == NULL) {
            free(email->extra);
            free(email);
            return NULL;
        }
    }

    return email;
}

int mail_setattr(struct mail *email, enum mail_attr attr, const char *data) {
    int data_len, i, t;
    data_len = strlen(data) + 1;
    t = 0;

    switch(attr) {
    case FROMS:
        /* ensure there is a server (if not, return error) */
        if (data_len == 0) return MAIL_ERROR_PARSE;

        /* copy the data */
        email->froms_c = data_len;
        email->froms_v = malloc(data_len);

        /* successful allocation check */
        if (email->froms_v == NULL) return MAIL_ERROR_OOM;

        /* copy the data */
        memcpy(email->froms_v, data, data_len);

        /* no error */
        return MAIL_ERROR_NONE;
    case FROM:
        /* ensure brackets around email (if not, return error) */
        if (!(*data == '<' && data[data_len-2] == '>')) return MAIL_ERROR_PARSE;

        /* ensure exactly at least one @ symbol present */
        for (i = 0; i < data_len-1; i++)
            if (data[i] == '@') t++;
        if (t < 1) return MAIL_ERROR_PARSE;

        /* for a FROM email, that's all the checking we will do. */
        email->from_c = data_len;
        email->from_v = malloc(data_len - 2); /* subtract the <> */

        /* successful allocation check */
        if (email->from_v == NULL) return MAIL_ERROR_OOM;

        /* copy the data, omitting the start '<' and end '\0' */
        memcpy(email->from_v, data + 1, data_len - 2);

        /* replace end '>' with '\0' */
        email->from_v[data_len - 3] = 0;

        /* no error */
        return MAIL_ERROR_NONE;
    default:
        /* should be calling mail_addattr() */
        return MAIL_ERROR_PROGRAM;
    }
}

int mail_addattr(struct mail *email, enum mail_attr attr, const char *data) {
    int data_len, i, t;
    data_len = strlen(data) + 1;
    t = 0;

    switch(attr) {
    case TO:
        /* ensure brackets around email (if not, return error) */
        if (!(*data == '<' && data[data_len-2] == '>')) return MAIL_ERROR_PARSE;

        /* TODO: ensure valid domain */

        /* ensure valid TO address */
        for (i = 0; i < data_len-1; i++) {
            if (data[i] == '@') t++;
            /* forward slashes not allowed */
            if (data[i] == '/') return MAIL_ERROR_INVALIDEMAIL;
        }
        if (t < 1) return MAIL_ERROR_PARSE;

        /* make sure we're not trying to append to a readonly email */
        if (email->extra == NULL) return MAIL_ERROR_PROGRAM;

        /* check there's enough room for this address */
        while (((email->extra)->to_total_len) < (email->to_c + data_len)) {
            void *x;
            /* reallocate memory */
            (email->extra)->to_total_len *= 2;

            /* ensure max bounds for memory allocation */
            if (((email->extra)->to_total_len) > MAIL_MAX_TO_C)
                return MAIL_ERROR_RCPTMAX;

            /* reallocation + error checking */
            x = realloc(email->to_v, (email->extra)->to_total_len);
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
    data_len = strlen(data) + 1;

    /* make sure we're not trying to append to a readonly email */
    if (email->extra == NULL) return MAIL_ERROR_PROGRAM;

    /* check there's enough room for this line of data (if not, realloc) */
    while (((email->extra)->data_total_len) < (email->data_c + data_len)) {
        void *x;
        /* reallocate memory */
        (email->extra)->data_total_len *= 2;

        /* ensure max bounds for memory allocation */
        if (((email->extra)->data_total_len) > MAIL_MAX_DATA_C)
            return MAIL_ERROR_DATAMAX;

        /* reallocation + error checking */
        x = realloc(email->data_v, (email->extra)->data_total_len);
        if (!x) return MAIL_ERROR_OOM;
        email->data_v = (char *)x;
    }

    /* copy data over */
    memcpy(email->data_v + email->data_c, data, data_len);

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
    free(email->extra);
    free(email);
}
    
void mail_serialize(struct mail *email, enum mail_sf format, int sock) {
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

    printf("------\num ok so I just got an email from socket %d!!!", sock);
    printf("here's some info about it:\n");
    printf("real sender server ip: `%s` rDNS:`%s`\n", ip, hst);
    printf("reported sender server: `%s`\n", email->froms_v);
    printf("reported sender email: `%s`\n", email->from_v);
    printf("reported recipients:\n");
    for (i = 0; i < email->to_c; i++) {
        printf("\t`%s`\n", email->to_v + i);
        i += strlen(email->to_v + i);
    }
    printf("data: ```\n%s```\n", email->data_v);
}
