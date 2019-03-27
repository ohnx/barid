#ifndef __NET_H_INC
#define __NET_H_INC

int net_close(struct client *client);
void net_deinit();
int net_init(const char *ssl_cert, const char *ssl_key);
int net_rx(struct client *client);
int net_sssl(struct client *client);
int net_tx(struct client *client, unsigned char *buf, size_t bufsz);

#endif /* __NET_H_INC */
