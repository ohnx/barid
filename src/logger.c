/* this file handles logging and stuff */
/* fprintf(), vfprintf() */
#include <stdio.h>
/* va_list, va_start(), va_end() */
#include <stdarg.h>
/* time_t, struct tm, time(), localtime() */
#include <time.h>
/* pthread_mutex_t, pthread_mutex_lock(), pthread_mutex_unlock() */
#include <pthread.h>
/* enum logger_level*/
#include "logger.h"
/* KBLU, RESET, etc. */
#include "common.h"

void logger_log(enum logger_level level, const char *fmt, ...) {
    va_list args;
    static time_t t;
    static struct tm *tm;
    static pthread_mutex_t logger_mutex = PTHREAD_MUTEX_INITIALIZER;

    /* lock the mutex for stderr */
    pthread_mutex_lock(&logger_mutex);

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

    /* unlock */
    pthread_mutex_unlock(&logger_mutex);
}
