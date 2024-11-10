#ifndef PUBSUB_H
#define PUBSUB_H

#define MAX_TOPICS 100
#define MAX_DATA_LEN 256
#define MAX_SUBSCRIBERS 10
#define MAX_QUEUE_SIZE 50

typedef enum { PUBLISH, SUBSCRIBE, PULL } MessageType;

typedef struct {
    MessageType type;
    char topic[50];
    char data[MAX_DATA_LEN];
} Message;

// Define a structure for subscribers
typedef struct {
    int socket; // Socket descriptor for the subscriber
} Subscriber;

// Define a structure for a topic
typedef struct {
    char name[50];              // Topic name
    Message queue[MAX_QUEUE_SIZE]; // Queue of messages for this topic
    int queue_size;             // Current size of the message queue
    Subscriber subscribers[MAX_SUBSCRIBERS]; // List of subscribers
    int subscriber_count;       // Current number of subscribers
} Topic;

// Function prototypes
Topic* find_or_create_topic(const char* topic_name);
void subscribe_client(Message msg, int client_sock);
void publish_message(Message msg);
void send_data(int client_sock, const char* topic);

#endif
