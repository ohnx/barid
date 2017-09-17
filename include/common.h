#ifndef __COMMON_H_INC
#define __COMMON_H_INC

#ifdef __linux__
#define KRED  "\x1B[31m"
#define KYEL  "\x1B[33m"
#define RESET "\033[0m"
#else
#define KRED  ""
#define KYEL  ""
#define RESET ""
#endif

#define WARN "["KYEL"!"RESET"] "
#define ERR "["KRED"!"RESET"] "

enum server_stage {
    HELO,
    MAIL,
    RCPT,
    DATA,
    END_DATA,
    QUIT
};

/* max length of an email (in bytes) - default is 16 MiB (16777216 B) */
#define MAIL_MAX_DATA_C      16777216

/* version string */
#define MAILVER "SMTP mail"

/* this is the buffer for a single line; mail size can be found in mail.h */
#define LARGEBUF        4096
/* this is the small buffer for output */
#define SMALLBUF        256


#endif /* __COMMON_H_INC */
