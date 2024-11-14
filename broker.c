#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "pubsub.h"

#define PORT 8080

// Map of registered users (in memory)
struct User {
    char id[50];
    char password[50];
} users[MAX_SUBSCRIBERS];
int user_count = 0;

Topic topics[MAX_TOPICS];
int topic_count = 0;

Subscriber subscribers[MAX_SUBSCRIBERS];
int subscriber_count = 0;

// Register a new subscriber
int register_subscriber(char* id, char* password) {
    for (int i = 0; i < subscriber_count; i++) {
        if (strcmp(subscribers[i].id, id) == 0) {
            return 0; // User already exists
        }
    }
    if (subscriber_count < MAX_SUBSCRIBERS) {
        strcpy(subscribers[subscriber_count].id, id);
        strcpy(subscribers[subscriber_count].password, password);
        subscribers[subscriber_count].topic_count = 0;
        subscriber_count++;
        return 1;
    }
    return 0; // Registration failed
}

// Login an existing subscriber
int login_subscriber(char* id, char* password, int sock) {
    for (int i = 0; i < subscriber_count; i++) {
        if (strcmp(subscribers[i].id, id) == 0 && strcmp(subscribers[i].password, password) == 0) {
            subscribers[i].socket = sock;
            return 1; // Login success
        }
    }
    return 0; // Login failed
}

// Add a topic to a subscriber's list
void add_topic_to_subscriber(char* id, char* topic) {
    for (int i = 0; i < subscriber_count; i++) {
        if (strcmp(subscribers[i].id, id) == 0) {
            strcpy(subscribers[i].topics[subscribers[i].topic_count], topic);
            subscribers[i].topic_count++;
        }
    }
}

// Send the list of subscribed topics to a subscriber
void send_subscribed_topics(int client_sock, char* id) {
    for (int i = 0; i < subscriber_count; i++) {
        if (strcmp(subscribers[i].id, id) == 0) {
            int topic_count = subscribers[i].topic_count;
            send(client_sock, &topic_count, sizeof(int), 0);
            for (int j = 0; j < topic_count; j++) {
                Message msg;
                msg.type = TOPIC_LIST;
                strcpy(msg.topic, subscribers[i].topics[j]);
                send(client_sock, &msg, sizeof(msg), 0);
            }
        }
    }
}

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
        add_topic_to_subscriber(msg.id, msg.topic);
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
    
    int opt = 1;
    // Set the SO_REUSEADDR option to allow port reuse
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Set SO_REUSEADDR failed");
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

int register_publisher(const char* id, const char* password) {
    for (int i = 0; i < user_count; ++i) {
        if (strcmp(users[i].id, id) == 0) {
            return 0; // User already exists
        }
    }
    if (user_count < MAX_SUBSCRIBERS) {
        strcpy(users[user_count].id, id);
        strcpy(users[user_count].password, password);
        user_count++;
        return 1;
    }
    return 0; // Registration failed
}

int login_publisher(const char* id, const char* password) {
    for (int i = 0; i < user_count; ++i) {
        if (strcmp(users[i].id, id) == 0 && strcmp(users[i].password, password) == 0) {
            return 1; // Login success
        }
    }
    return 0; // Login failed
}

void handle_auth(Message msg, int client_sock) {
    int response;

    if (strcmp(msg.user_type, "subscriber") == 0) {
        if (strcmp(msg.action, "register") == 0) {
            response = register_subscriber(msg.id, msg.password);
        } else if (strcmp(msg.action, "login") == 0) {
            response = login_subscriber(msg.id, msg.password, client_sock);
        } else {
            response = 0; // Invalid action
        }
    } else if (strcmp(msg.user_type, "publisher") == 0) {
        if (strcmp(msg.action, "register") == 0) {
            response = register_publisher(msg.id, msg.password);
        } else if (strcmp(msg.action, "login") == 0) {
            response = login_publisher(msg.id, msg.password);
        } else {
            response = 0; // Invalid action
        }
    } else {
        response = 0; // Invalid user type
    }

    send(client_sock, &response, sizeof(response), 0);
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
            case AUTH:
                handle_auth(msg, client_sock);
                break;
            case SUBSCRIBE:
                subscribe_client(msg, client_sock);
                break;
            case PUBLISH:
                publish_message(msg);
                break;
            case PULL:
                send_data(client_sock, msg.topic);
                break;
            case TOPIC_LIST:
                send_subscribed_topics(client_sock, msg.id);
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
