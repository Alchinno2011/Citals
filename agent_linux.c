#include <stdio.h>
#include <sys/statvfs.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/stat.h>

#define SERVER_IP "100.64.1.46"
#define SERVER_PORT 2000
#define GUID_DIR  "/var/lib/vitals"
#define GUID_PATH GUID_DIR "/guid"

float get_total_storage_size_in_gigabytes(const char *path);
float get_free_storage_size_in_gigabytes(const char *path);
float get_used_storage_size_in_gigabytes(const char *path);
int send_data_to_server(int fd, const float free_storage_size, const float used_storage_size, const float total_storage_size);
int connect_socket();
char *ask_for_guid(int fd);
char *read_file();
void write_file(char *guid);
char *check_guid(int fd);

int main() {
    printf("Free storage space: %0.2f GB\n", get_free_storage_size_in_gigabytes("/"));
    printf("Used storage space: %0.2f GB\n", get_used_storage_size_in_gigabytes("/"));
    printf("Total storage space: %0.2f GB\n", get_total_storage_size_in_gigabytes("/"));

    int fd = connect_socket();

    char *guid = check_guid(fd);

    free(guid);

    //send_data_to_server(fd, get_free_storage_size_in_gigabytes("/"), get_used_storage_size_in_gigabytes("/"), get_total_storage_size_in_gigabytes("/"));
    return 0;
}


int connect_socket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("connect");
        close(fd);
        exit("server not reachable");
        return 1;
    }

    printf("Connected to server %s:%d\n", SERVER_IP, SERVER_PORT);

    return fd;
}


int send_data_to_server(int fd, const float free_storage_size, const float used_storage_size, const float total_storage_size) {

    char body[200];
    int body_len = snprintf(body, sizeof body, "{\"free_storage_size\": %0.2f, \"used_storage_size\": %0.2f, \"total_storage_size\": %0.2f}", free_storage_size, used_storage_size, total_storage_size);

    char msg[256];
    int len = snprintf(msg, sizeof msg, "{\"type\": \"report\", \"messagelen\": %d, \"body\": %s}\n", body_len, body);

    printf("Sending data to %s:%d: %s\n", SERVER_IP, SERVER_PORT, msg);

    if (send(fd, msg, strlen(msg), 0) < 0) {
        perror("send");
    }

    if (get_ack(fd) != 0) {
        fprintf(stderr, "Failed to receive ack from server\n");
    } else {
        printf("Received ack from server\n");
    }

    close(fd);
    return 0;
}


char *check_guid(int fd) {
    char *guid;

    if (access(GUID_PATH, F_OK) != 0) {
        printf("guid file doesn't exist\n");
        char *server_guid = ask_for_guid(fd);
        printf("server guid is: %s\n", server_guid);
        write_file(server_guid);
        return server_guid;
    } else {
        printf("guid file exists\n");
        char *file_guid = read_file(fd);
        if (file_guid == NULL) {
            printf("guid is empty\n");
            char *server_guid = ask_for_guid(fd);
            printf("server guid is: %s\n", server_guid);
            write_file(server_guid);
            return server_guid;
        } else {
            printf("guid is: %s\n", file_guid);
            return file_guid;
        }
    }
}

void write_file(char *guid) {

    if (guid == NULL) {
        printf("guid is NULL, not writing to file\n");
        return;
    }

    mkdir(GUID_DIR, 0755); //make file path if it doesn't exist

    printf("Writing guid to file: %s\n", GUID_PATH);

    FILE *fptr;

    fptr = fopen(GUID_PATH, "w");
    if (fptr == NULL) {
        printf("Error opening file for writing at: %s\n", GUID_PATH);
        return;
    }

    printf("Writing guid to %s: %s\n", GUID_PATH, guid);

    fprintf(fptr, "%s", guid);
    fclose(fptr);
}

char *read_file() {
    FILE *fptr;

    fptr = fopen(GUID_PATH, "r");
    
    char *guid = malloc(128);

    fgets(guid, 128, fptr);

    if (strlen(guid) == 0 || guid == NULL) {
        printf("guid is empty or doesn't exist\n");
        fclose(fptr);
        free(guid);
        return NULL;
    }

    fclose(fptr);

    return guid;
}


char *ask_for_guid(int fd) {
    char *guid = malloc(128);
    char get_guid_req[256];

    int len = snprintf(get_guid_req, sizeof get_guid_req, "{\"type\": \"get_guid\", \"messagelen\": %d, \"body\": ""}\n", 0);

    printf("Sending get_guid request to server: %s\n", get_guid_req);

    send(fd, get_guid_req, len, 0);


    char response[1024];
    int response_len = recv(fd, response, sizeof response, 0);
    int messagelen;
    char type[20];

    int response_match = sscanf(response, "{\"type\": \"%20[^\"]\", \"messagelen\": %d, \"body\": {\"guid\": \"%127[^\"]\"}}\n", type, &messagelen, guid);


    if (strcmp(type, "give_guid") != 0 || response_match != 3) {
        printf("response_match: %d\n", response_match);
        fprintf(stderr, "ERROR bad response: %s\n", response);
        exit("bad response");
    } else {
        printf("Received guid from server: %s\n", guid);
        send_ack(fd);
        return guid;
    }

}


float  get_free_storage_size_in_gigabytes(const char *path) {
    struct statvfs s;
    statvfs(path, &s);
    unsigned long long free_space = (unsigned long long)s.f_bfree * s.f_frsize;
    return (float)free_space / 1073741824; // Convert bytes to GB
}

float  get_used_storage_size_in_gigabytes(const char *path) {
    struct statvfs s;
    statvfs(path, &s);
    unsigned long long used_space = (unsigned long long)(s.f_blocks - s.f_bfree) * s.f_frsize;
    return (float)used_space / 1073741824; // Convert bytes to GB
}

float  get_total_storage_size_in_gigabytes(const char *path) {
    struct statvfs s;
    statvfs(path, &s);
    unsigned long long total_space = (unsigned long long)s.f_blocks * s.f_frsize;
    return (float)total_space / 1073741824; // Convert bytes to GB
}