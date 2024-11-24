#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "pubsub.h"

#define BUFFER_SIZE 1024
#define LOAD_BALANCER_IP "127.0.0.1"
#define LOAD_BALANCER_PORT 9000

void connect_to_load_balancer(int *sock, struct sockaddr_in *lb_addr) {
    if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    lb_addr->sin_family = AF_INET;
    lb_addr->sin_port = htons(LOAD_BALANCER_PORT);
    inet_pton(AF_INET, LOAD_BALANCER_IP, &lb_addr->sin_addr);

    if (connect(*sock, (struct sockaddr *)lb_addr, sizeof(*lb_addr)) < 0) {
        perror("Connection to load balancer failed");
        close(*sock);
        exit(EXIT_FAILURE);
    }
}

void create_new_topic(int sock) {
    char topic[BUFFER_SIZE];
    Message msg;

    printf("Enter the name of the new topic: ");
    fgets(topic, sizeof(topic), stdin);
    topic[strcspn(topic, "\n")] = 0; 

    msg.type = CREATE_TOPIC;
    strcpy(msg.topic, topic);

    send(sock, &msg, sizeof(msg), 0);
    char received_char;
    recv(sock, &received_char, sizeof(received_char), 0);
    if(received_char == '1')
        printf("Topic '%s' created successfully.\n", topic);
    else
        printf("Topic '%s' already exists!\n", msg.topic);
    
    close(sock);
}

void publish_new_message(int sock) {
    char topic[BUFFER_SIZE], message[BUFFER_SIZE];
    Message msg;

    printf("Enter topic to publish to: ");
    fgets(topic, sizeof(topic), stdin);
    topic[strcspn(topic, "\n")] = 0; 

    printf("Enter the message to publish: ");
    fgets(message, sizeof(message), stdin);
    message[strcspn(message, "\n")] = 0; 

    msg.type = PUBLISH;
    strcpy(msg.topic, topic);
    strcpy(msg.data, message);

    send(sock, &msg, sizeof(msg), 0);
    char received_char;
    recv(sock, &received_char, sizeof(received_char), 0);
    if(received_char == '1')
        printf("Message published to topic '%s'.\n", topic);
    else
        printf("Topic '%s' does NOT exist. First create the topic then retry!\n", topic);

    close(sock);
}

void publisher() {
    int sock;
    struct sockaddr_in lb_addr;

    while (1) {
        printf("\n--- Publisher Menu ---\n");
        printf("1. Create a new topic\n");
        printf("2. Publish a new message\n");
        printf("3. Exit\n");
        printf("Select an option: ");

        int choice;
        scanf("%d", &choice);
        getchar(); 

        switch (choice) {
            case 1:
                connect_to_load_balancer(&sock, &lb_addr);
                create_new_topic(sock);
                break;
            case 2:
                connect_to_load_balancer(&sock, &lb_addr);
                publish_new_message(sock);
                break;
            case 3:
                close(sock);
                printf("Exiting Publisher...\n");
                return;
            default:
                printf("Invalid option. Please try again.\n");
        }

        close(sock); 
    }
}

int main() {
    printf("Starting Publisher...\n");
    publisher();
    return 0;
}
