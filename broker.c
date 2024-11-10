#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "pubsub.h"

#define PORT 8080

Topic topics[MAX_TOPICS];
int topic_count = 0;

// Find or create a topic by name
Topic* find_or_create_topic(const char* topic_name) {
    for (int i = 0; i < topic_count; ++i) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            return &topics[i];
        }
    }
    if (topic_count < MAX_TOPICS) {
        strcpy(topics[topic_count].name, topic_name);
        topics[topic_count].queue_size = 0;
        topics[topic_count].subscriber_count = 0;
        return &topics[topic_count++];
    }
    return NULL;
}

// Subscribe client to a topic
void subscribe_client(Message msg, int client_sock) {
    Topic* topic = find_or_create_topic(msg.topic);
    if (topic && topic->subscriber_count < MAX_SUBSCRIBERS) {
        topic->subscribers[topic->subscriber_count++].socket = client_sock;
        printf("Client subscribed to topic: %s\n", msg.topic);
    } else {
        printf("Failed to subscribe client to topic: %s\n", msg.topic);
    }
}

// Publish message to a topic
void publish_message(Message msg) {
    Topic* topic = find_or_create_topic(msg.topic);
    if (topic == NULL) {
        printf("Topic creation failed or max topics reached\n");
        return;
    }
    if (topic->queue_size >= MAX_QUEUE_SIZE) {
        printf("Failed to publish message to topic %s: queue is full\n", msg.topic);
        return;
    }
    topic->queue[topic->queue_size++] = msg;
    printf("Message published to topic: %s\n", msg.topic);

    for (int i = 0; i < topic->subscriber_count; ++i) {
        if (send(topic->subscribers[i].socket, &msg, sizeof(msg), 0) < 0) {
            perror("Failed to send message to subscriber");
        }
    }
}

// Send data to a subscriber on request
void send_data(int client_sock, const char* topic_name) {
    Topic* topic = find_or_create_topic(topic_name);
    if (topic == NULL) {
        printf("Topic %s not found\n", topic_name);
        return;
    }
    for (int i = 0; i < topic->queue_size; ++i) {
        if (send(client_sock, &topic->queue[i], sizeof(topic->queue[i]), 0) < 0) {
            perror("Failed to send data to client");
            return;
        }
    }
    printf("All messages sent to subscriber for topic: %s\n", topic_name);
}

int setup_server_socket() {
    int sock;
    struct sockaddr_in server_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
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

void* handle_client(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);

    while (1) {
        Message msg;
        int bytes_received = recv(client_sock, &msg, sizeof(msg), 0);
        if (bytes_received <= 0) {
            printf("Client disconnected\n");
            close(client_sock);
            return NULL;
        }
        switch (msg.type) {
            case SUBSCRIBE:
                subscribe_client(msg, client_sock);
                break;
            case PUBLISH:
                publish_message(msg);
                break;
            case PULL:
                send_data(client_sock, msg.topic);
                break;
            default:
                break;
        }
    }
}

int main() {
    int server_sock = setup_server_socket();
    printf("Broker server started on port %d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);

        if (*client_sock < 0) {
            perror("Accept failed");
            free(client_sock);
            continue;
        }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_sock);
        pthread_detach(tid);
    }
    close(server_sock);
    return 0;
}
