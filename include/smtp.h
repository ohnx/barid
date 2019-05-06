#ifndef __SMTP_H_INC_H
#define __SMTP_H_INC_H

/* struct client, struct barid_conf */
#include "common.h"

int smtp_gendynamic(struct barid_conf *sconf);
void smtp_freedynamic();
int smtp_handlecode(struct client *client, int code);

#endif /* __SMTP_H_INC_H */
