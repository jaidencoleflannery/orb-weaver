#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include "types/thread-types/thread_types.h"
#include "services/logging/logging.h"
#include "configuration/configuration-handler/configuration_handler.h"
#include "utilities/error-handler/error_handler.h"
#include "services/connection-handler/connection_handler.h"
#include "services/http-handler/http_handler.h"

#include "./thread_handler.h"

static bool initialized = false;

static thread_instance *threads; // holds all threads contiguously.
static pthread_mutex_t thread_lock;
static pthread_cond_t thread_lock_available;
static pthread_mutex_t enqueue_lock;
static pthread_cond_t enqueue_lock_available;
static connection_instance *queue_head; // linked list of connections < seems like this wasnt used, consider removing.
static connection_instance **queue_tail = &queue_head;
static int count = 0;

/*
 * thread_handler implements the producer consumer pattern.
 * the producer is the queue of connections, which the threads pull from.
 * whichever threads holds the mutex is waiting for a connection to arrive in the queue, 
 * once a connection arrives, it drops the mutex and starts processing that connection.
 * all other threads wait for the mutex to become available, and repeat the aforementioned behavior. 
 */

bool enqueue_task(int client_descriptor) {
    if(!initialized) {
        ERROR_LOG("queue_task: Service has not been initialized.");
        return false;
    }

    // enqueue_task is not multi-threaded, this is just to guarantee safety + delay for observers.
    pthread_mutex_lock(&enqueue_lock); 

    DEBUG_LOG("enqueue_task: Queuing task.");

    (*queue_tail)->next = calloc(1, sizeof(connection_instance));
    if((*queue_tail)->next == NULL) {
        ERROR_LOG("enqueue_task: Failed to allocate memory for task.");
        return false;
    }

    *(*queue_tail)->next = (connection_instance){ 0 };

    (*queue_tail)->next->previous = *queue_tail;
    queue_tail = &(*queue_tail)->next;
    (*queue_tail)->socket_descriptor = client_descriptor;

    ++count; 
    DEBUG_LOG("enqueue_task: Task successfully queued, total tasks: %d.", count); 
 
    pthread_mutex_unlock(&enqueue_lock);
    pthread_cond_signal(&enqueue_lock_available); // notify waiting threads. 

    return true;
}

// returns the file descriptor for the connection.
bool pull_next_task(int *result) {
    if(!initialized) {
        ERROR_LOG("pull_next_task: Service has not been initialized.");
        return false;
    }

    DEBUG_LOG("pull_next_task: Attempting to pull task from queue.");

    connection_instance *task = queue_head->next;
    if(task == NULL) {
        ERROR_LOG("pull_next_task: Failure, no task found.");
        return false;
    }

    *result = task->socket_descriptor;
    if(result == NULL || *result < 0) {
        ERROR_LOG("pull_next_task: Failure, socket descriptor in queue was invalid.");
        *result = -1;
        return false;
    }

    // pop node.
    if(queue_head->next->socket_descriptor == (*queue_tail)->socket_descriptor) {
        queue_head->next = NULL;
        queue_tail = &queue_head;
    } else {
        queue_head->next = queue_head->next->next;
        queue_head->next->previous = queue_head;
    }

    // TODO: make sure this isn't messing up *result.
    free(task);
    --count;
    return true;
}

static bool process_request(int socket_descriptor) {
    if(!initialized) {
        ERROR_LOG("process_request: Service has not been initialized.");
        return false;
    }

    DEBUG_LOG("process_request: Processing request on socket: %d.", socket_descriptor);
 
    // TODO: need to loop on this until the message is finished and store it properly.
    char *buffer = calloc(1, RECEIVE_BUFFER_SIZE); // leaving room for terminator.
    char child_buffer[RECEIVE_BUFFER_SIZE] = { 0 };
    size_t total_bytes_read = 0;
    while(1) {
        size_t num_bytes_read = 0;
        memset(child_buffer, 0, RECEIVE_BUFFER_SIZE);
        if(!receive_data(socket_descriptor, 0, (RECEIVE_BUFFER_SIZE - 2), child_buffer, &num_bytes_read)) {
            ERROR_LOG("process_request: Failed to receive data.");
            return false;
        }

        LOG("[ READ ]", "%zu bytes.", num_bytes_read); 

        if((total_bytes_read + num_bytes_read) >= RECEIVE_BUFFER_SIZE) {
            ERROR_LOG("process_request: Request exceed maximum request size of %d.", RECEIVE_BUFFER_SIZE);
            return false;
        } 

        *(buffer + total_bytes_read) = *child_buffer;
        total_bytes_read += num_bytes_read;

        // end of data.
        if(num_bytes_read < 1) {
            DEBUG_LOG("process_request: Hit end of message.");
            break;
        }
    }

    buffer[RECEIVE_BUFFER_SIZE - 1] = '\0';

    char *response_buffer = calloc(64, sizeof(char));
    if(!process_http_request(socket_descriptor, buffer, RECEIVE_BUFFER_SIZE, &response_buffer)) {
        ERROR_LOG("process_request: Failed to invoke response.");
        return false;
    }

    LOG("[ RESPONSE ]", "%s\n", response_buffer);
    return true;
}

static void *thread_runner(void *client_descriptor) {
    DEBUG_LOG("thread_runner: Waiting for a connection to join the queue.");

    while(1) { 
        pthread_mutex_lock(&thread_lock); 

        while(count == 0) {
            DEBUG_LOG("thread_runner: Hit thread condition.");
            pthread_cond_wait(&thread_lock_available, &thread_lock);
        }

        DEBUG_LOG("thread_runner: Value found in queue.");

        int *socket_descriptor = calloc(1, sizeof(int));
        *socket_descriptor = -1;
        if(!pull_next_task(socket_descriptor)) {
            ERROR_LOG("thread_runner: Failed to fetch socket_descriptor from queue.");
            pthread_mutex_unlock(&thread_lock);
            return NULL;
        }

        pthread_mutex_unlock(&thread_lock);
        DEBUG_LOG("thread_runner: Released lock.");
        
        if(!process_request(*socket_descriptor)) {
            ERROR_LOG("thread_runner: Unable to process request.");
            close(*socket_descriptor);
            return NULL;
        }
    }

    return NULL;
}

// thread pool is a linked list of threads.
bool init_thread_handler(void) {
    DEBUG_LOG("init_thread_handler: Initializing thread handler service.");

    // initialize mutex values.
    if(pthread_mutex_init(&thread_lock, NULL) != 0) {
        ERROR_LOG("init_thread_handler: Fatal error, failed to initialize thread mutex.");
        return false;
    }

    if(pthread_cond_init(&thread_lock_available, NULL) != 0) {
        ERROR_LOG("init_thread_handler: Fatal error, failed to initialize thread mutex condition.");
        return false;
    }

    if(pthread_mutex_init(&enqueue_lock, NULL) != 0) {
        ERROR_LOG("init_thread_handler: Fatal error, failed to initialize enqueue mutex.");
        return false;
    }

    if(pthread_cond_init(&enqueue_lock_available, NULL) != 0) {
        ERROR_LOG("init_thread_handler: Fatal error, failed to initialize enqueue mutex condition.");
        return false;
    }

    // prealloc pool based on num of performance cores.
    threads = calloc(1, (config.num_cores * sizeof(thread_instance)));
    if(threads == NULL) {
        ERROR_LOG("init_thread_handler: Fatal error, failed to allocate memory for thread pool.");
        return false;
    }

    // start threads and track values.
    for(int cursor = 0; cursor < config.num_cores; cursor++) {
        *(threads + cursor) = (thread_instance){
            .virtual_id = cursor,
            .thread_id = (pthread_t)-1 // the actual thread's id. 
        };

        if(!validate_syscall(
            pthread_create(&(threads + cursor)->thread_id, NULL, thread_runner, NULL),
            "start_processing",
            "Could not create a thread for processing.")
        ) { return false; }

        if((threads + cursor)->thread_id == (pthread_t)-1) {
            ERROR_LOG("init_thread_handler: Fatal error, failed to create thread.");
            return false;
        }
    }
 
    // initialize queue of connections.
    queue_head = (connection_instance *)calloc(1, sizeof(connection_instance));
    if(queue_head == NULL) {
        ERROR_LOG("init_thread_handler: Failed to allocate memory for queue.");
        return false;
    }

    *queue_head = (connection_instance){ 0 };
    queue_tail = &queue_head;

    initialized = true;
    return true;
}

