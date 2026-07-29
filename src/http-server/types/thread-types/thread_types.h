#ifndef THREAD_TYPES_H
#define THREAD_TYPES_H

typedef struct thread {
    size_t virtual_id;
    pthread_t thread_id;
} thread_instance;

typedef struct connection {
    int socket_descriptor;
    struct connection *next;
    struct connection *previous;
} connection_instance;

#endif

