#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include "pubsub.h"

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    char topic[BUFFER_SIZE];
} Subscriber;

Subscriber subscribers[MAX_CLIENTS];
int subscriber_count = 0;
pthread_mutex_t subscriber_lock = PTHREAD_MUTEX_INITIALIZER;

void add_subscriber(int fd, const char *topic) {
    pthread_mutex_lock(&subscriber_lock);
    subscribers[subscriber_count].fd = fd;
    strcpy(subscribers[subscriber_count].topic, topic);
    subscriber_count++;
    pthread_mutex_unlock(&subscriber_lock);
}

void remove_subscriber(int fd) {
    pthread_mutex_lock(&subscriber_lock);
    for (int i = 0; i < subscriber_count; i++) {
        if (subscribers[i].fd == fd) {
            subscribers[i] = subscribers[subscriber_count - 1];
            subscriber_count--;
            break;
        }
    }
    pthread_mutex_unlock(&subscriber_lock);
}

void broadcast_message(const char *topic, const char *message, int sender_fd) {
    pthread_mutex_lock(&subscriber_lock);
    for (int i = 0; i < subscriber_count; i++) {
        if (strcmp(subscribers[i].topic, topic) == 0 && subscribers[i].fd != sender_fd) {
            send(subscribers[i].fd, message, strlen(message), 0);
        }
    }
    printf("Message broadcasted to all the subscribers of the topic '%s'\n", topic);
    pthread_mutex_unlock(&subscriber_lock);
}

void broker(int port) {
    int server_fd, max_fd, activity;
    struct sockaddr_in address;
    fd_set read_fds, temp_fds;
    char buffer[BUFFER_SIZE];

    memset(subscribers, 0, sizeof(subscribers));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Broker started on port %d\n", port);

    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    max_fd = server_fd;

    while (1) {
        temp_fds = read_fds;
        activity = select(max_fd + 1, &temp_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            break;
        }

        if (FD_ISSET(server_fd, &temp_fds)) {
            socklen_t addrlen = sizeof(address);
            int new_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            if (new_fd < 0) {
                perror("Accept failed");
                continue;
            }

            printf("New connection accepted (fd: %d)\n", new_fd);
            FD_SET(new_fd, &read_fds);
            if (new_fd > max_fd) {
                max_fd = new_fd;
            }
        }

        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &temp_fds) && fd != server_fd) {
                int bytes_read = recv(fd, buffer, BUFFER_SIZE - 1, 0);

                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        printf("Client disconnected (fd: %d)\n", fd);
                    } else {
                        perror("Recv failed");
                    }
                    close(fd);
                    FD_CLR(fd, &read_fds);
                    remove_subscriber(fd);
                } else {
                    buffer[bytes_read] = '\0';
                    Message msg;
                    memcpy(&msg, buffer, sizeof(msg));

                    if (msg.type == SUBSCRIBE) {
                        printf("Subscriber for topic '%s' connected (fd: %d)\n", msg.topic, fd);
                        add_subscriber(fd, msg.topic);

                    } else if (msg.type == PUBLISH) {
                        printf("Message received for topic '%s': %s\n", msg.topic, msg.data);
                        broadcast_message(msg.topic, msg.data, fd);
                    }
                }
            }
        }
    }

    close(server_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    broker(port);

    return 0;
}
