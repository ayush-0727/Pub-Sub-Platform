#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include "pubsub.h"

#define PORT 9000
#define MAX_BROKERS 10
#define MAX_TOPICS 100
#define BUFFER_SIZE 1024
#define MAX_QUEUE_SIZE 100

void handle_request(int client_sock);

typedef struct {
    char ip[BUFFER_SIZE];
    int port;
} Broker;

Broker brokers[MAX_BROKERS];
int broker_count = 0;
int current_broker = 0;

typedef struct {
    char topic[BUFFER_SIZE];
    char broker_ip[BUFFER_SIZE];
    int broker_port;
} TopicBrokerMapping;

TopicBrokerMapping topic_mapping[MAX_TOPICS];
int topic_count = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

pthread_t *thread_pool;
int thread_pool_size;
int request_queue[MAX_QUEUE_SIZE];
int front = 0, rear = 0, queue_size = 0;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

void enqueue_request(int client_sock) {
    pthread_mutex_lock(&lock);
    while (queue_size == MAX_QUEUE_SIZE) {
        pthread_cond_wait(&cond_var, &lock);
    }
    request_queue[rear] = client_sock;
    rear = (rear + 1) % MAX_QUEUE_SIZE;
    queue_size++;
    pthread_cond_broadcast(&cond_var);
    pthread_mutex_unlock(&lock);
}

int dequeue_request() {
    pthread_mutex_lock(&lock);
    while (queue_size == 0) {
        pthread_cond_wait(&cond_var, &lock);
    }
    int client_sock = request_queue[front];
    front = (front + 1) % MAX_QUEUE_SIZE;
    queue_size--;
    pthread_cond_broadcast(&cond_var);
    pthread_mutex_unlock(&lock);
    return client_sock;
}

void *worker_thread(void *arg) {
    while (1) {
        int client_sock = dequeue_request();
        handle_request(client_sock);
        close(client_sock);
    }
}

void add_broker(const char *ip, int port) {
    strcpy(brokers[broker_count].ip, ip);
    brokers[broker_count].port = port;
    broker_count++;
    printf("Broker added %s %d\n", ip, port);
}

Broker get_next_broker() {
    pthread_mutex_lock(&lock);
    Broker selected_broker = brokers[current_broker];
    current_broker = (current_broker + 1) % broker_count;
    pthread_mutex_unlock(&lock);
    return selected_broker;
}

int find_broker_for_topic(const char *topic, Broker *broker) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topic_mapping[i].topic, topic) == 0) {
            strcpy(broker->ip, topic_mapping[i].broker_ip);
            broker->port = topic_mapping[i].broker_port;
            pthread_mutex_unlock(&lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&lock);
    return -1; 
}

void assign_broker_to_topic(const char *topic, Broker *broker) {
    *broker = get_next_broker(); 
    pthread_mutex_lock(&lock);
    strcpy(topic_mapping[topic_count].topic, topic);
    strcpy(topic_mapping[topic_count].broker_ip, broker->ip);
    topic_mapping[topic_count].broker_port = broker->port;
    topic_count++;
    pthread_mutex_unlock(&lock);
    printf("Assigned topic '%s' to broker %s:%d\n", topic, broker->ip, broker->port);
}

void forward_message_to_broker(Message *msg, const Broker *broker) {
    int sock;
    struct sockaddr_in broker_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return;
    }

    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(broker->port);
    inet_pton(AF_INET, broker->ip, &broker_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("Connect to broker failed");
        close(sock);
        return;
    }

    send(sock, msg, sizeof(*msg), 0);
    printf("Forwarded message to broker %s:%d for topic '%s'\n", broker->ip, broker->port, msg->topic);

    close(sock);
}

void handle_request(int client_sock) {
    Message msg;
    recv(client_sock, &msg, sizeof(msg), 0);
    char send_char;

    Broker broker;
    if (msg.type == CREATE_TOPIC) {
        printf("\nPUBLISHER: CREATING NEW TOPIC = '%s'\n", msg.topic);
        if(find_broker_for_topic(msg.topic, &broker) == -1) {
            assign_broker_to_topic(msg.topic, &broker);
            send_char = '1';
            send(client_sock, &send_char, sizeof(send_char), 0);
        }
        else{
            send_char = '0';
            send(client_sock, &send_char, sizeof(send_char), 0);
        }
    } else if (msg.type == PUBLISH) {
        if(find_broker_for_topic(msg.topic, &broker) == -1) {
            send_char = '0';
            send(client_sock, &send_char, sizeof(send_char), 0);
        } else {
            send_char = '1';
            send(client_sock, &send_char, sizeof(send_char), 0);
            forward_message_to_broker(&msg, &broker);
        }
    } else if (msg.type == SUBSCRIBE) {
        if (find_broker_for_topic(msg.topic, &broker) == -1) {
            const char *error_msg = "Topic not available";
            send(client_sock, error_msg, strlen(error_msg), 0);
        } else {
            char response[BUFFER_SIZE];
            sprintf(response, "BROKER %s %d", broker.ip, broker.port);
            send(client_sock, response, strlen(response), 0);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <thread_pool_size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    thread_pool_size = atoi(argv[1]);
    thread_pool = malloc(thread_pool_size * sizeof(pthread_t));

    add_broker("10.130.154.45", 8085);
    add_broker("10.130.154.35", 8081);
    add_broker("10.130.154.47", 8082);

    for (int i = 0; i < thread_pool_size; i++) {
        pthread_create(&thread_pool[i], NULL, worker_thread, NULL);
    }

    int server_sock, client_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, MAX_BROKERS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Load balancer started on port %d\n", PORT);

    while (1) {
        if ((client_sock = accept(server_sock, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }
        enqueue_request(client_sock);
    }

    return 0;
}
