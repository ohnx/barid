/* this file handles logging and stuff */
/* fprintf(), vfprintf() */
#include <stdio.h>
/* va_list, va_start(), va_end() */
#include <stdarg.h>
/* time_t, struct tm, time(), localtime() */
#include <time.h>
/* enum logger_level*/
#include "logger.h"
/* KBLU, RESET, etc. */
#include "common.h"

void logger_log(enum logger_level level, const char *fmt, ...) {
    va_list args;
    time_t t;
    struct tm *tm;

    /* time info */
    time(&t);
    tm = localtime(&t);

    /* level */
    switch (level) {
    case INFO: fprintf(stderr, "["KBLU"!"RESET"]"); break;
    case WARN: fprintf(stderr, "["KYEL"!"RESET"]"); break;
    default: fprintf(stderr, "["KRED"!"RESET"]"); break;
    }

    /* time */
    fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* user-defined string */
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
