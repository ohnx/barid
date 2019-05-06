#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
    int sfd;
    struct sockaddr_in server;
    FILE *f;
    long *len;
    char **data;
    int i;

    if (argc < 2) {
        puts("Usage: ./writefile <filename> <filename>");
        return -1;
    }

    data = (char **)malloc(sizeof(char *) * argc);
    len = (long *)malloc(sizeof(long) * argc);

    for (i = 1; i < argc; i++) {
        f = fopen(argv[i], "rb");
        if (!f) {
            printf("%s is a bad file >:(\n", argv[i]);
            return -1;
        }

        fseek(f, 0, SEEK_END);
        len[i] = ftell(f);
        fseek(f, 0, SEEK_SET);

        data[i] = malloc(len[i]);
        fread(data[i], len[i], sizeof(char), f);
        fclose(f);
    }

    puts("read all files!");

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        puts("rip socket");
        return -1;
    }
    else
        puts("socket ok");
    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(25);

    if (connect(sfd, (struct sockaddr *)&server, sizeof(server)) != 0) {
        puts("rip server connect");
        return -1;
    }
    else
        printf("connected to the server..\n");

    for (i = 1; i < argc; i++) {
        if (write(sfd, data[i], len[i]) != len[i]) {
            printf("oof couldn't write all of %s\n", argv[i]);
        }
        free(data[i]);
    }

    free(data);
    free(len);
    puts("wrote all the data!");

    sleep(1);
    close(sfd);

    puts("closed socket; done!");
    return 0;
}
