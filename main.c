#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/random.h>

#define PORT 2000
#define SERVER_ADDR "100.64.1.46"
#define MAX_NUMBER_OF_CLIENTS 10
#define GUID_BYTES   16
#define GUID_STR_LEN 37


int shutdown(int sockfd, int how);
char *generate_guid(char *out);


int main() {

    int server_socket;
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0){
        printf("Creating the socket failed (%d)\n\n", server_socket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_ADDR, &server_address.sin_addr);

    bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    listen(server_socket, MAX_NUMBER_OF_CLIENTS);

    printf("Server is listening on %s:%d\n", SERVER_ADDR, PORT);


    handle_connection(server_socket);

    close(server_socket);

    return 0;
}



void handle_connection(int server_socket) {
    while (1) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            printf("Accepting the connection failed (%d)\n\n", client_socket);
            exit(1);
            continue;
        }

        printf("Accepted a connection from a client\n");

        char buffer[1024] = {0};
        read(client_socket, buffer, sizeof(buffer) - 1);
        printf("Received data from client: %s\n", buffer);


        float free_storage_size, used_storage_size, total_storage_size;
        char type[20];
        int messagelen;

        int match = sscanf(buffer, "{\"type\": \"%19[^\"]\", \"messagelen\": %d, \"body\": {\"free_storage_size\": %f, \"used_storage_size\": %f, \"total_storage_size\": %f}}\n", type, &messagelen, &free_storage_size, &used_storage_size, &total_storage_size);

        printf("type: %s\n", type);

        if (strcmp(type, "report") != 0 && strcmp(type, "get_guid") != 0) {
            fprintf(stderr, "bad message: %s\n", buffer);
        } else {
            if (strcmp(type, "report") == 0) {

                send_ack(client_socket);

                printf("Free storage size: %0.2f GB\n", free_storage_size);
                printf("Used storage size: %0.2f GB\n", used_storage_size);
                printf("Total storage size: %0.2f GB\n", total_storage_size);
            } else if (strcmp(type, "get_guid") == 0) {

                char guid[GUID_STR_LEN];

                generate_guid(guid);

                printf("Generated guid: %s\n", guid);

                char msg[256];
                char body[200];

                int body_len = snprintf(body, sizeof body, "{\"guid\": \"%s\"}", guid);
                int len = snprintf(msg, sizeof msg, "{\"type\": \"%s\", \"messagelen\": %d, \"body\": %s}\n", "give_guid", body_len, body);

                printf("Sending guid to client: %s\n", msg);

                send(client_socket, msg, len, 0);

                if (get_ack(client_socket) != 0) {
                    fprintf(stderr, "Failed to receive ack from client\n");
                } else {
                    printf("Received ack from client\n");
                }
            }
        }
    }
}

char *generate_guid(char *out) {

    uint8_t bytes[GUID_BYTES];


    if (getrandom(bytes, sizeof bytes, 0) != sizeof bytes) {
        printf("Failed to generate random bytes for GUID\n");
        return NULL;
    }


    bytes[6] = (bytes[6] & 0x0f) | 0x40;  /* version 4 */
    bytes[8] = (bytes[8] & 0x3f) | 0x80;  /* variant 10xx */


    // Formating the bytes into a GUID string
    snprintf(out, GUID_STR_LEN,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2],  bytes[3],
             bytes[4], bytes[5],
             bytes[6], bytes[7],
             bytes[8], bytes[9],
             bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);

    return out;

}

