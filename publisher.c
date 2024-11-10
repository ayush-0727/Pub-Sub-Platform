#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "pubsub.h"

#define PORT 8080

int setup_client_socket() {
    int sock;
    struct sockaddr_in server_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }
    return sock;
}

void publish_to_topic(int sock, char* topic, char* data) {
    Message msg;
    msg.type = PUBLISH;
    strcpy(msg.topic, topic);
    strcpy(msg.data, data);
    send(sock, &msg, sizeof(msg), 0);
}

int main() {
    int sock = setup_client_socket();

    char topic[50];
    char data[256];
    printf("Enter topic: ");
    scanf("%s", topic);
    printf("Enter message: ");
    scanf("%s", data);

    publish_to_topic(sock, topic, data);
    close(sock);
    return 0;
}
