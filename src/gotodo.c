/*

this is some real spaghetti code right here 

in one file, we speak:

* http to send gotodo a request
* json to communicate with the gotodo api
* markdown to pretty print the results
* mbox to parse the emails
* mime (ish) to parse the emails

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <curl/curl.h>

#include "jansson.h"

#include "common.h"
#include "mail.h"
#include "logger.h"

#define ALLOWED_EMAILS 4
const char *allowed_emails[] = {
    "me@masonx.ca",
    "mmx@andrew.cmu.edu",
    "masonx.01@gmail.com",
    "mason.x01@gmail.com"
};

int gotodo_parse_and_submit(CURL *curl, struct mail *email, const char *account) {
    (void)account;
    char *s = NULL, *line = NULL, *next_line = NULL;
    json_t *root, *todo_item;

#define PARSER_MULTIPART 3
#define PARSER_DATA      1
#define PARSER_INITIAL   0
    char *todo_title = NULL, *message_id = NULL, *fwd_id = NULL, *message_body = NULL, *due_date = NULL;
    int parsing_state = PARSER_INITIAL;

    /* step 1 - verify from allowed senders */
    for (int i = 0; i < ALLOWED_EMAILS; i++) {
        if (!strncasecmp(email->from_v, allowed_emails[i], email->from_c))
            goto emails_ok;
    }

    logger_log(WARN, "%s does not match allowed senders\n", email->from_v);
    /* sender sad */
    return -1;

emails_ok:

    /* step 2 - read the mail line by line */
    line = email->data_v;
    while (line) {
        next_line = memchr(line, '\n', email->data_c - (line - email->data_v));
        if (next_line) *next_line = '\0';

        switch (parsing_state) {
        case PARSER_INITIAL:
            /* handle stuff */
            if (!strncasecmp("Message-Id: ", line, 12)) {
                /* incoming message id */
                if (message_id) free(message_id);
                message_id = strdup(line + 12);
            } else if (!strncasecmp("References: ", line, 12)) {
                /* forwarded message id */
                if (fwd_id) free(fwd_id);
                fwd_id = strdup(line + 12);
            } else if (!(*line)) {
                /* empty line = moving to data */
                parsing_state = PARSER_DATA;
            }
            break;
        case PARSER_DATA:
            if (!strncmp("--", line, 2)) {
                /* entering mime multipart or ending a multipart */
                if (message_body) goto done;
                else parsing_state = PARSER_MULTIPART;
            } else if (*line == '>') {
                /* done parsing, we've reached the forwarded message */
                goto done;
            } else if (*line) { /* skip empty lines */
                /* add more to this message body */
                if (!todo_title) {
                    /* no title, allocate it! */
                    todo_title = strdup(line);
                } else {
                    /* check for due date */
                    // 2022-08-25T13:00:00.000Z
                    if (!strncasecmp("Due ", line, 4)) {
                        /* due date expected format is %m/%d %I:%M%p */
                        struct tm tm;
                        time_t t;
                        int month = 0;

                        /* populate with localtime first */
                        t = time(NULL);
                        localtime_r(&t, &tm);
                        month = tm.tm_mon;
                        tm.tm_sec = 0;

                        if (!strptime(line + 4, "%m/%d %I:%M%p", &tm)) {
                            printf("very sad, strp failed on %s\n", line + 4);
                        } else {
                            /* advance to next year if needed */
                            if (tm.tm_mon < month) tm.tm_year++;

                            /* output the due date */
                            /* convert time zone */
                            t = mktime(&tm);
                            gmtime_r(&t, &tm);

                            /* again, idk how long exactly it is, but 32 chars is enough */
                            char *tmp = malloc(sizeof(char) * 32);
                            if (tmp) {
                                if (strftime(tmp, 32, "%FT%H:%M:%S.000Z", &tm)) {
                                    if (due_date) free(due_date);
                                    due_date = tmp;
                                } else {
                                    free(tmp);
                                }
                            }
                        }
                    } else if (message_body) {
                        /* hopefully most todo items aren't too many lines so this awful way isn't too bad */
                        int old_length = strlen(message_body);
                        int new_length = strlen(line);
                        char *tmp = malloc((new_length + old_length + 2)*sizeof(char));
                        if (tmp) {
                            strncpy(tmp, message_body, old_length);
                            tmp[old_length] = '\n';
                            strncpy(tmp + old_length + 1, line, new_length);
                            tmp[old_length + new_length + 1] = '\0';
                            free(message_body);
                            message_body = tmp;
                        }
                    } else {
                        message_body = strdup(line);
                    }
                }
            }
            break;
        case PARSER_MULTIPART:
            if (!strncasecmp("Content-Type: text/plain", line, 24)) {
                /* ready to look for another empty line to start data */
                parsing_state = PARSER_INITIAL;
            }
            break;
        default:
            break;
        }

        if (next_line) *next_line = '\n';
        line = next_line ? (next_line+1) : NULL;
        continue;
    done:
        break;
   }

    if (!todo_title) todo_title = strdup("Item from email");

    if (message_id) {
        /* 34 should be more than enough, idk how long it is exactly tbh */
        int bufamount = strlen(message_id) + 34;
        char *new_msgid = malloc(bufamount * sizeof(char));
        if (new_msgid) {
            if (bufamount > snprintf(new_msgid, bufamount, "[Genesis email](message://%s\\)", message_id)) {
                free(message_id);
                message_id = NULL;
                /* need to swap the > and \ to escape - silly weird issue */
                message_id = strrchr(new_msgid, '>');
                *message_id = '\\';
                message_id[1] = '>';
                message_id = new_msgid;
            } else {
                free(new_msgid);
            }
        }
    } else {
        message_id = strdup("");
    }

    if (fwd_id) {
        /* 34 should be more than enough, idk how long it is exactly tbh */
        int bufamount = strlen(fwd_id) + 34;
        char *new_fwdid = malloc(bufamount * sizeof(char));
        if (new_fwdid) {
            if (bufamount > snprintf(new_fwdid, bufamount, "[Reference email](message://%s\\)\n", fwd_id)) {
                free(fwd_id);
                fwd_id = NULL;
                /* need to swap the > and \ to escape - silly weird issue */
                fwd_id = strrchr(new_fwdid, '>');
                *fwd_id = '\\';
                fwd_id[1] = '>';
                fwd_id = new_fwdid;
            } else {
                free(new_fwdid);
            }
        }
    } else {
        fwd_id = strdup("");
    }

    if (!due_date) {
        /* use the same constant due date in the far, far, future */
        due_date = strdup("2222-01-01T00:00:00.000Z");
    }

    /* step 3 - produce the json object */
    root = json_object();
    todo_item = json_object();

    json_object_set_new(root, "authority", json_string(getenv("GOTODO_TOKEN")));
    json_object_set_new(root, "todo", todo_item);

    json_object_set_new(todo_item, "id", json_integer(-1));
    json_object_set_new(todo_item, "state", json_integer(1));
    json_object_set_new(todo_item, "tag_id", json_integer(1));
    json_object_set_new(todo_item, "public", json_false());
    json_object_set_new(todo_item, "name", json_string(todo_title));
    json_object_set_new(todo_item, "due_date", json_string(due_date));
    json_object_set_new(todo_item, "description", json_sprintf("%s\n\n%s%s", message_body, fwd_id, message_id));

    s = json_dumps(root, 0);
    json_decref(root);

    /* step 4: submit it to the server! */
    curl_easy_setopt(curl, CURLOPT_URL, getenv("GOTODO_ENDPOINT"));
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(s));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, s);
    curl_easy_perform(curl);

    free(s);

    goto cleanup;

cleanup:
    if (todo_title) free(todo_title);
    if (message_id) free(message_id);
    if (fwd_id) free(fwd_id);
    if (message_body) free(message_body);
    if (due_date) free(due_date);

    return 0;
}

#ifdef GOTODO_TEST

int main(int argc, char **argv) {
    CURL *curl;
    int i;
    FILE *fp;
    struct mail *tester;
    char buf[LARGEBUF];
    struct barid_conf *conf;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    conf = malloc(sizeof(struct barid_conf));
    conf->domains = "*";
    conf->accounts = "*";
    mail_set_allowed(conf);
    free(conf);

    for (i = 1; i < argc; i++) {
        printf("%s:\n", argv[i]);
        fp = fopen(argv[i], "r");
        if (!fp) continue;

        tester = mail_new(NULL);
        if (mail_setattr(tester, FROMS, "example.com")) fprintf(stderr, "%d: sad\n", __LINE__);
        if (mail_setattr(tester, FROM, "<person1@example.com>")) fprintf(stderr, "%d: sad\n", __LINE__);
        if (mail_addattr(tester, TO, "<person2@example.com>")) fprintf(stderr, "%d: sad\n", __LINE__);

        while (fgets(buf, sizeof(buf), fp)) {
            mail_appenddata(tester, buf);
        }
        fclose(fp);

        (void)gotodo_parse_and_submit(curl, tester, NULL);

        mail_destroy(tester);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
}

#endif
