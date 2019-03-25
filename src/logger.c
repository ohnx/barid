/* this file handles logging and stuff */
/* fprintf(), vfprintf() */
#include <stdio.h>
/* va_list, va_start(), va_end() */
#include <stdarg.h>
/* enum logger_level*/
#include "logger.h"
/* sconf */
#include "common.h"

void logger_log(enum logger_level level, const char *fmt, ...) {
    va_list args;

    switch (level) {
    case INFO: fprintf(sconf.logger_fd, "["KBLU"!"RESET"]"); break;
    case WARN: fprintf(sconf.logger_fd, "["KYEL"!"RESET"]"); break;
    default: fprintf(sconf.logger_fd, "["KRED"!"RESET"]"); break;
    }
    va_start(args, fmt);
    vfprintf(sconf.logger_fd, fmt, args);
    va_end(args);
    fprintf(sconf.logger_fd, "\n");
}
