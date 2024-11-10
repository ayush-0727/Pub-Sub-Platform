#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "pubsub.h"

// For setting up a server socket
int setup_server_socket() {
    int sock;
    struct sockaddr_in server_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sock, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    return sock;
}

// For setting up a client socket
int setup_client_socket() {
    int sock;
    struct sockaddr_in server_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8080);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }

    return sock;
}


void subscribe_to_topic(int sock, char* topic) {
    Message msg;
    msg.type = PULL;
    strcpy(msg.topic, topic);
    send(sock, &msg, sizeof(msg), 0);
}

int main() {
    int sock = setup_client_socket();

    char topic[50];
    printf("Enter topic to subscribe: ");
    scanf("%s", topic);

    subscribe_to_topic(sock, topic);

    // Handle incoming messages for subscribed topics
    Message msg;
    while (1) {
        int bytes_received = recv(sock, &msg, sizeof(msg), 0);
        if (bytes_received <= 0) {
            perror("Disconnected from broker or error");
            break;
        }

        if (msg.type == PUBLISH) {
            printf("Received message on topic %s: %s\n", msg.topic, msg.data);
        }
        usleep(1000); // Small delay to avoid overwhelming output
    }

    close(sock);
    return 0;
}
