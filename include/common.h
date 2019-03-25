#ifndef __COMMON_H_INC
#define __COMMON_H_INC

/* FILE */
#include <stdio.h>
/* sig_atomic_t */
#include <signal.h>

struct barid_conf {
    FILE *logger_fd;
};

/* max length of an email's recipients (in bytes)
 * 512 recipients @ 256 bytes per email = 131072 B (128 KiB) */
#define MAIL_MAX_TO_C           131072

/* max length of an email (in bytes) - default is 16777216 B (16 MiB) */
#define MAIL_MAX_DATA_C         16777216

/* version string */
#define MAILVER "barid v1.0.0a"

/* buffer for a single line of input from a server */
#define LARGEBUF                4096

/* server configuration */
extern struct barid_conf sconf;

/* running flag */
extern sig_atomic_t running;

#endif /* __COMMON_H_INC */
