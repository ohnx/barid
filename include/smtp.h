#ifndef __SMTP_H_INC_H
#define __SMTP_H_INC_H

/* struct client */
#include "common.h"

int smtp_gengreeting();
int smtp_handlecode(struct client *client, int code);

#endif /* __SMTP_H_INC_H */
