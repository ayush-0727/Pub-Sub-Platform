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
    server_addr.sin_addr.s_addr = inet_addr("10.130.154.45");
    server_addr.sin_port = htons(PORT);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }
    return sock;
}

// Function to send login or registration info to the broker
int authenticate(int sock, char* id, char* password, char* action) {
    Message msg;
    msg.type = AUTH;
    strcpy(msg.action, action);  // "login" or "register"
    strcpy(msg.id, id);
    strcpy(msg.password, password);
    strcpy(msg.user_type, "publisher");

    send(sock, &msg, sizeof(msg), 0);

    // Receive broker's response (success or failure)
    int response;
    recv(sock, &response, sizeof(response), 0);
    return response;  // 1 for success, 0 for failure
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
    char action[10], id[50], password[50];
    int authenticated = 0;

    while (!authenticated) {
        printf("\nEnter action (login/register): ");
        scanf("%s", action);
        printf("Enter Publisher ID: ");
        scanf("%s", id);
        printf("Enter Password: ");
        scanf("%s", password);

        authenticated = authenticate(sock, id, password, action);
        if (!authenticated) {
            printf("\nAuthentication failed. Please try again.\n");
        } else {
            printf("\nAuthentication successful.\n");
        }
    }

    char topic[50], data[256];
    while(1)
    {
        printf("\nEnter topic to publish: ");
        scanf("%s", topic);
        printf("Enter message: ");
        scanf("%s", data);

        publish_to_topic(sock, topic, data);
    }
    close(sock);
    return 0;
}
