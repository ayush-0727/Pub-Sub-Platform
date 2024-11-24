#ifndef PUBSUB_H
#define PUBSUB_H

#define MAX_TOPICS 100
#define MAX_DATA_LEN 256
#define MAX_SUBSCRIBERS 10

typedef enum { PUBLISH, SUBSCRIBE, CREATE_TOPIC } MessageType;

typedef struct {
    MessageType type;
    char topic[50];
    char data[MAX_DATA_LEN];
} Message;

#endif
