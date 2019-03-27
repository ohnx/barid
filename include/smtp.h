#ifndef __SMTP_H_INC_H
#define __SMTP_H_INC_H

#include "common.h"
#include "ssl.h"
#include "server.h"

int smtp_gengreeting();
int smtp_handlecode(struct client *client, int code);

#endif /* __SMTP_H_INC_H */
