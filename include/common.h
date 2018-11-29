#ifndef __COMMON_H_INC
#define __COMMON_H_INC

#ifdef __linux__
#define KRED  "\x1B[31m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define RESET "\033[0m"
#else
#define KRED  ""
#define KYEL  ""
#define RESET ""
#endif

#define INFO "["KBLU"!"RESET"]"
#define WARN "["KYEL"!"RESET"]"
#define ERR "["KRED"!"RESET"]"

/* server stage */
enum server_stage {
    HELO,
    MAIL,
    RCPT,
    DATA,
    END_DATA,
    QUIT
};

/* this struct holds internal info */
struct mail_internal_info {
    int to_total_len;
    int data_total_len;
    unsigned char using_ssl;
    struct sockaddr_storage *origin_ip;
};

struct mail {
    /* the server that this mail is from */
    int froms_c;
    char *froms_v;
    /* the email address that this mail is from */
    int from_c;
    char *from_v;
    /* email addresses this email is to */
    int to_c; /* total length of email address string */
    char *to_v; /* null-separated array of to email addresses */
    /* message data */
    int data_c;
    char *data_v;
    /* extra information (a null `extra` indicates this email is READONLY) */
    struct mail_internal_info *extra;
};

struct common_data {
    char *server_greeting;
    int server_greeting_len;
    char *server_hostname;
};

/* max length of an email's recipients (in bytes)
 * 512 recipients @ 256 bytes per email = 131072 B (128 KiB) */
#define MAIL_MAX_TO_C           131072

/* max length of an email (in bytes) - default is 16777216 B (16 MiB) */
#define MAIL_MAX_DATA_C         16777216

/* version string */
#define MAILVER "barid v0.3.1j"

/* buffer for a single line of input from a server */
#define LARGEBUF                4096

#endif /* __COMMON_H_INC */
