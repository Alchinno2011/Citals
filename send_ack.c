#include "send_ack.h"
#include <stdio.h>
#include <string.h>

int send_ack(int fd) {
    char msg[256];
    int len = snprintf(msg, sizeof msg, "{\"type\": \"ack\", \"messagelen\": %d, \"body\": ""}\n", 0);
    printf("Sending ack: %s\n", msg);

    send(fd, msg, len, 0);
    return 0;
}

int get_ack(int fd) {
    char response[1024];

    int response_len = recv(fd, response, sizeof response, 0);

    if (response[response_len - 1] != '\n') {
        printf("ERROR: client did not receive a complete response from the server\n");
    }

    printf("Received ack: %.*s\n", response_len, response);


    char type[20];
    int messagelen;

    int match = sscanf(response, "{\"type\": \"%19[^\"]\", \"messagelen\": %d, \"body\": ""}\n", type, &messagelen);

    if (strcmp(type, "ack") != 0 || match != 2) {
        printf("match: %d\n", match);
        fprintf(stderr, "ERROR bad response: %s\n", response);
        return -1;
    } else {
        return 0;
    }
}