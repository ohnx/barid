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

/* max length of an email's recipients (in bytes)
 * 1024 recipients @ 256 bytes per email = 262144 B (256 KiB) */
#define MAIL_MAX_TO_C           262144

/* max length of an email (in bytes) - default is 16777216 B (16 MiB) */
#define MAIL_MAX_DATA_C         16777216

/* version string */
#define MAILVER "SMTP mail"

/* buffer for a single line of input from a server */
#define LARGEBUF                4096

#endif /* __COMMON_H_INC */
