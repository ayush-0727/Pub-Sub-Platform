#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include "pubsub.h"

#define BUFFER_SIZE 1024
#define LOAD_BALANCER_IP "127.0.0.1"
#define LOAD_BALANCER_PORT 9000

typedef struct {
    char broker_ip[BUFFER_SIZE];
    int broker_port;
    char topic[50];
} BrokerInfo;

typedef struct {
    int broker_sock;
    char topic[50];
} BrokerConnection;

BrokerConnection broker_connections[MAX_TOPICS];
int connection_count = 0;

BrokerInfo get_broker_info(const char *topic) {
    int sock;
    struct sockaddr_in lb_addr;
    BrokerInfo broker_info = {"", 0};
    Message msg;
    char response[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    lb_addr.sin_family = AF_INET;
    lb_addr.sin_port = htons(LOAD_BALANCER_PORT);
    inet_pton(AF_INET, LOAD_BALANCER_IP, &lb_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&lb_addr, sizeof(lb_addr)) < 0) {
        perror("Connection to load balancer failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    msg.type = SUBSCRIBE;
    strcpy(msg.topic, topic);
    send(sock, &msg, sizeof(msg), 0);

    int bytes = recv(sock, response, sizeof(response) - 1, 0);
    response[bytes] = '\0';
    close(sock);

    sscanf(response, "BROKER %s %d", broker_info.broker_ip, &broker_info.broker_port);
    strcpy(broker_info.topic, topic);

    return broker_info;
}

int connect_to_broker(const BrokerInfo *broker_info) {
    int broker_sock;
    struct sockaddr_in broker_addr;

    if ((broker_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(broker_info->broker_port);
    inet_pton(AF_INET, broker_info->broker_ip, &broker_addr.sin_addr);

    if (connect(broker_sock, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("Connection to broker failed");
        close(broker_sock);
        return -1;
    }

    Message msg;
    msg.type = SUBSCRIBE;
    strcpy(msg.topic, broker_info->topic);
    send(broker_sock, &msg, sizeof(msg), 0);

    printf("Subscribed to topic '%s' at broker %s:%d\n",
           broker_info->topic, broker_info->broker_ip, broker_info->broker_port);

    return broker_sock;
}

void listen_for_messages() {
    fd_set read_fds;
    char buffer[BUFFER_SIZE];
    int max_fd = 0;

    printf("Listening for messages (press ENTER to return to main menu)...\n");

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); 
        max_fd = STDIN_FILENO;

        for (int i = 0; i < connection_count; i++) {
            FD_SET(broker_connections[i].broker_sock, &read_fds);
            if (broker_connections[i].broker_sock > max_fd) {
                max_fd = broker_connections[i].broker_sock;
            }
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            printf("Returning to main menu...\n");
            break;
        }

        for (int i = 0; i < connection_count; i++) {
            int sock = broker_connections[i].broker_sock;
            if (FD_ISSET(sock, &read_fds)) {
                int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    printf("[Topic: %s] %s\n", broker_connections[i].topic, buffer);
                } else {
                    printf("Disconnected from broker for topic '%s'\n",
                           broker_connections[i].topic);
                    close(sock);
                    FD_CLR(sock, &read_fds);
                }
            }
        }
    }
}

void show_subscribed_topics() {
    if (connection_count == 0) {
        printf("No topics subscribed yet.\n");
        return;
    }

    printf("\n--- Subscribed Topics ---\n");
    for (int i = 0; i < connection_count; i++) {
        printf("%d. %s\n", i + 1, broker_connections[i].topic);
    }
    printf("-------------------------\n");
}

int main() {
    while (1) {
        printf("\n--- Subscriber Menu ---\n");
        printf("1. Subscribe to a topic\n");
        printf("2. Listen for new messages\n");
        printf("3. View subscribed topics\n");
        printf("4. Exit\n");
        printf("Enter your choice: ");

        int choice;
        scanf("%d", &choice);
        getchar(); 

        if (choice == 1) {
            if (connection_count >= MAX_TOPICS) {
                printf("Maximum number of topics reached.\n");
                continue;
            }

            char topic[BUFFER_SIZE];
            printf("Enter topic to subscribe to: ");
            fgets(topic, sizeof(topic), stdin);
            topic[strcspn(topic, "\n")] = 0; 

            BrokerInfo broker_info = get_broker_info(topic);
            if (broker_info.broker_port != 0) {
                int broker_sock = connect_to_broker(&broker_info);
                if (broker_sock >= 0) {
                    broker_connections[connection_count].broker_sock = broker_sock;
                    strcpy(broker_connections[connection_count].topic, topic);
                    connection_count++;
                }
            } else {
                printf("No broker available for topic '%s'\n", topic);
            }
        } else if (choice == 2) {
            if (connection_count == 0) {
                printf("No topics subscribed yet. Subscribe to a topic first.\n");
                continue;
            }
            listen_for_messages();
        } else if (choice == 3) {
            show_subscribed_topics();
        } else if (choice == 4) {
            printf("Exiting subscriber.\n");
            for (int i = 0; i < connection_count; i++) {
                close(broker_connections[i].broker_sock);
            }
            break;
        } else {
            printf("Invalid choice. Try again.\n");
        }
    }
    return 0;
}
