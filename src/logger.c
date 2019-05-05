/* this file handles logging and stuff */
/* fprintf(), vfprintf() */
#include <stdio.h>
/* va_list, va_start(), va_end() */
#include <stdarg.h>
/* enum logger_level*/
#include "logger.h"
/* KBLU, RESET, etc. */
#include "common.h"

void logger_log(enum logger_level level, const char *fmt, ...) {
    va_list args;

    switch (level) {
    case INFO: fprintf(stderr, "["KBLU"!"RESET"] "); break;
    case WARN: fprintf(stderr, "["KYEL"!"RESET"] "); break;
    default: fprintf(stderr, "["KRED"!"RESET"] "); break;
    }
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
