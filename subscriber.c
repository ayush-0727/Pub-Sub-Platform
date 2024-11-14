#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "pubsub.h"

#define PORT 8080

// Function to set up a client socket
int setup_client_socket() {
    int sock;
    struct sockaddr_in server_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("10.130.154.45");  // Broker address
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
    strcpy(msg.user_type, "subscriber");

    send(sock, &msg, sizeof(msg), 0);

    int response;
    recv(sock, &response, sizeof(response), 0);  // Receive success or failure
    return response;
}

// Function to retrieve subscribed topics from the broker
void retrieve_subscribed_topics(int sock, char* id) {
    Message msg;
    msg.type = TOPIC_LIST;
    strcpy(msg.id, id);
    send(sock, &msg, sizeof(msg), 0);

    // Receive the list of subscribed topics from the broker
    int topic_count;
    recv(sock, &topic_count, sizeof(int), 0);
    printf("\nPreviously subscribed topics:\n");
    for (int i = 0; i < topic_count; i++) {
        memset(&msg, 0, sizeof(msg));
        recv(sock, &msg, sizeof(msg), 0);
        printf(" - %s\n", msg.topic);
    }
}

// Function to subscribe to a new topic
void subscribe_to_topic(int sock, char* topic) {
    Message msg;
    msg.type = SUBSCRIBE;
    strcpy(msg.topic, topic);
    send(sock, &msg, sizeof(msg), 0);
}

int main() {
    int sock = setup_client_socket();
    char action[10], id[50], password[50];
    int authenticated = 0;

    // Login or register
    while (!authenticated) {
        printf("\nEnter action (login/register): ");
        scanf("%s", action);
        printf("Enter ID: ");
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

    // Retrieve and display previously subscribed topics
    if(strcmp(action, "register"))
        retrieve_subscribed_topics(sock, id);

    // Main loop: subscribe to topics or wait for messages
    while (1) {
        char topic[50];
        printf("\nEnter a topic to subscribe, or type 'wait' to wait for messages: ");
        scanf("%s", topic);

        if (strcmp(topic, "wait") == 0) {
            Message msg;
            while (1) {
                int bytes_received = recv(sock, &msg, sizeof(msg), 0);
                if (bytes_received <= 0) {
                    perror("Disconnected from broker or error");
                    break;
                }
                if (msg.type == PUBLISH) {
                    printf("Received message on topic %s: %s\n", msg.topic, msg.data);
                    printf("\n");
                }
                usleep(1000);
            }
        } else {
            subscribe_to_topic(sock, topic);
            usleep(1000);
        }
    }

    close(sock);
    return 0;
}
