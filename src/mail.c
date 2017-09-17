#include "mail.h"

/*https://serverfault.com/tags/mbox/info*/

struct mail *mail_new() {
    return NULL;
}

int mail_setattr(struct mail *email, enum mail_attr attr, const char *data) {
    return 0;
}

int mail_addattr(struct mail *email, enum mail_attr attr, const char * data) {
    return 0;
}

int mail_appenddata(struct mail *email, const char *data) {
    return 0;
}

void mail_clear(struct mail *email) {
    
}

void mail_serialize(struct mail *email) {
    
}
