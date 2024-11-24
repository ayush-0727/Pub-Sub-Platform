CC = gcc
CFLAGS = -Wall -pthread

BROKER = broker
LOADBALANCER = loadbalancer
PUBLISHER = publisher
SUBSCRIBER = subscriber

all: $(BROKER) $(LOADBALANCER) $(PUBLISHER) $(SUBSCRIBER)

$(BROKER): broker.c pubsub.h
	$(CC) $(CFLAGS) -o $@ broker.c

$(LOADBALANCER): loadbalancer.c pubsub.h
	$(CC) $(CFLAGS) -o $@ loadbalancer.c

$(PUBLISHER): publisher.c pubsub.h
	$(CC) -o $@ publisher.c

$(SUBSCRIBER): subscriber.c pubsub.h
	$(CC) -o $@ subscriber.c

clean:
	rm -f $(BROKER) $(LOADBALANCER) $(PUBLISHER) $(SUBSCRIBER)

.PHONY: all clean
