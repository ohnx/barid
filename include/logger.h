#ifndef __LOGGER_H_INC
#define __LOGGER_H_INC

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

enum logger_level {
    INFO,
    WARN,
    ERR
};

void logger_log(enum logger_level level, const char *fmt, ...);

#endif /* __LOGGER_H_INC */
