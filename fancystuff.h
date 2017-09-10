#ifndef __FANCYSTUFF_H_INC
#define

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

typedef char flag;

#endif /*  */