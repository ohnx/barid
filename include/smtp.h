#ifndef __SMTP_H_INC_H
#define __SMTP_H_INC_H

#include "common.h"
#include "server.h"

int smtp_handlecode(int code, int fd);
int smtp_parsel(char *line, enum server_stage *stage, struct mail *mail);

#endif /* __SMTP_H_INC_H */
