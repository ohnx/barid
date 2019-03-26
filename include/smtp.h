#ifndef __SMTP_H_INC_H
#define __SMTP_H_INC_H

#include "common.h"
#include "ssl.h"
#include "server.h"

int smtp_gengreeting();
int smtp_handlecode(int code, struct connection *conn);
int smtp_parsel(char *line, enum server_stage *stage, struct mail *mail);

#endif /* __SMTP_H_INC_H */
