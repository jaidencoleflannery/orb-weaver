#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "types/thread-types/thread_types.h"
#include "services/logging/logging.h"
#include "configuration/parsers/configuration-handler/configuration_handler.h"
#include "utilities/error-handler/error_handler.h"
#include "services/tcp/connection-handler/connection_handler.h"
#include "services/http/http-handler/http_handler.h"

#include "./thread_handler.h"

static thread_instance *threads; // holds all threads contiguously.
static pthread_mutex_t thread_lock;
static pthread_cond_t thread_lock_available;
static pthread_mutex_t enqueue_lock;
static connection_instance *queue_head;
static connection_instance **queue_tail = &queue_head;
static int num_connections = 0;

static bool process_request(int socket_descriptor);
static bool pull_next_task(int result);
static bool get_request_headers(char *buffer, int socket_descriptor, int32_t *total_bytes_read);
static bool get_request_contents(char *buffer, int socket_descriptor, char *content_length_header, int32_t *total_byts_read);

/*
 * thread_handler implements the producer consumer pattern.
 * the producer is the queue of connections, which the threads pull from.
 * whichever threads holds the mutex is waiting for a connection to arrive in the queue, 
 * once a connection arrives, it drops the mutex and starts processing that connection.
 * all other threads wait for the mutex to become available, and repeat the aforementioned behavior. 
*/

/*
 * + runtime per thread.
*/
static void *thread_runner(void *arg) { 
    // thread rejoins queue when finished with prior task.
    while(1) {
        DEBUG_LOG("thread_runner: Thread %lu is waiting.", (unsigned long)pthread_self());

        // a single thread waits on the queue at a time.
        // once an event is enqueued, it grabs it, releases the lock, and begins processing the task.

        pthread_mutex_lock(&thread_lock); 

            while(num_connections == 0) {
                DEBUG_LOG("thread_runner: Thread %lu found no active events, sleeping until signal is received.", (unsigned long)pthread_self());
                pthread_cond_wait(&thread_lock_available, &thread_lock);
            }

            DEBUG_LOG("thread_runner: Thread %lu was awoken - acting on task in queue.", (unsigned long)pthread_self());

            int socket_descriptor = -1;
            if(!pull_next_task(socket_descriptor)) {
                ERROR_LOG("thread_runner: Failed to fetch socket_descriptor from queue, error encountered on thread %lu.", (unsigned long)pthread_self());
                pthread_mutex_unlock(&thread_lock);
                if(socket_descriptor != -1)
                    close(socket_descriptor);
                continue;
            }

        pthread_mutex_unlock(&thread_lock);
        
        DEBUG_LOG("thread_runner: Processing task %d.", socket_descriptor);
        if(!process_request(socket_descriptor)) {
            ERROR_LOG("thread_runner: Unable to process request on thread %lu.", (unsigned long)pthread_self());
            if(socket_descriptor != -1)
                close(socket_descriptor);
            continue;
        }

        // thread is finished, clean up.
        if(socket_descriptor != -1)
            close(socket_descriptor);
    }

    return NULL;
}

/*
 * + adds a task to the queue.
*/
bool enqueue_task(uintptr_t client_descriptor) {
    // guarantee queue ordering.
    pthread_mutex_lock(&enqueue_lock);

        // task to tail.

        (*queue_tail)->next = calloc(1, sizeof(connection_instance));
        if((*queue_tail)->next == NULL) {
            ERROR_LOG("enqueue_task: Failed to allocate memory for task.");
            pthread_mutex_unlock(&enqueue_lock);
            return false;
        }

        *(*queue_tail)->next = (connection_instance){ 0 };

        (*queue_tail)->next->previous = *queue_tail;
        queue_tail = &(*queue_tail)->next;
        (*queue_tail)->socket_descriptor = client_descriptor;

        // shared between locks.
        ++num_connections; 
        DEBUG_LOG("enqueue_task: Task successfully queued, total tasks: %d.", num_connections);

    pthread_mutex_unlock(&enqueue_lock);
    // ring the dinner bell (wake up waiting thread).
    pthread_cond_signal(&thread_lock_available);
  
    return true;
}

bool free_thread_memory(void) {
    if(threads != NULL)
        free(threads);
    if(queue_head != NULL)
        free(queue_head);

    return true;
}

/*
 * + fetch the foremost task from the queue.
 * returns the file descriptor for the connection via the address parameter.
*/
static bool pull_next_task(int result) {
    // guarantee queue ordering.
    pthread_mutex_lock(&enqueue_lock);

        connection_instance *task = queue_head->next;
        if(task == NULL) {
            ERROR_LOG("pull_next_task: Failure, no task found.");
            pthread_mutex_unlock(&enqueue_lock);
            return false;
        }

        result = task->socket_descriptor;
        if(result < 0) {
            ERROR_LOG("pull_next_task: Failure, socket descriptor in queue was invalid, clearing task from queue.");
            result = -1; // pop + free node and return a placeholder node via following logic.
        }

        // pop node.
        if(task == *queue_tail) {
            queue_head->next = NULL;
            queue_tail = &queue_head;
        } else {
            queue_head->next = queue_head->next->next;
            queue_head->next->previous = queue_head;
        }

        free(task);
        --num_connections;

    pthread_mutex_unlock(&enqueue_lock); 

    // signal validity of data to caller.
    return (result > -1);
}

/*
 * + load headers and body into buffer.
 * + pass unvalidated data straight to processor.
*/
static bool process_request(int socket_descriptor) {
    char *buffer = calloc(1, RECEIVE_BUFFER_SIZE);
    if(buffer == NULL) {
        ERROR_LOG("process_request: Error, failed to allocate memory for buffer on thread %lu.", (unsigned long)pthread_self());
        return false;
    }

    int32_t total_bytes_read = 0;

    if(!get_request_headers(buffer, socket_descriptor, &total_bytes_read)) {
        ERROR_LOG("process_request: Error, failed to parse header from request from socket %d on thread %lu.", socket_descriptor, (unsigned long)pthread_self());
        return false;
    }

    char *content_length_header = strstr(buffer, CONTENT_LENGTH_HEADER);
    // conditional incase of get request.
    if(content_length_header != NULL) {
        if(!get_request_contents(buffer, socket_descriptor, content_length_header, &total_bytes_read)) {
            ERROR_LOG("process_request: Error, failed to parse header from request from socket %d on thread %lu.", socket_descriptor, (unsigned long)pthread_self());
            return false;
        }
    }

    DEBUG_LOG("process_request: Message received, attempting to process request.");

    http_request *response_buffer = calloc(1, MAX_RESPONSE_SIZE);
    if(response_buffer == NULL) {
        ERROR_LOG("process_request: Failed to allocate memory for response buffer.");
        return false;
    }

    if(!process_http_request(socket_descriptor, buffer, total_bytes_read, &response_buffer)) {
        ERROR_LOG("process_request: Failed to invoke response on thread %lu.", (unsigned long)pthread_self());
        if(response_buffer != NULL)
            free(response_buffer);
        if(buffer != NULL)
            free(buffer);
        return false;
    }

    // TODO: this needs to actually send the response back to the client.
    // this is a debug value, should be wholly removed.
    // for sending values, loop over the entire payload and write() to the socket descriptor in chunks (the kernel or nic handles packet segmentation).
    DEBUG_LOG("checkpoint reached. remove this log");

    if(response_buffer != NULL)
        free(response_buffer);
    if(buffer != NULL)
        free(buffer);

    return true;
}

/*
 * thread pool is a linked list of threads.
 * each thread sits on the task queue and waits for a task to be enqueued.
*/
bool init_thread_handler(void) {
    DEBUG_LOG("init_thread_handler: Initializing thread handler service.");

    // initialize mutex values.
    if(pthread_mutex_init(&thread_lock, NULL) != 0) {
        ERROR_LOG("init_thread_handler: Fatal error, failed to initialize thread mutex.");
        return false;
    }

    if(pthread_mutex_init(&enqueue_lock, NULL) != 0) {
        ERROR_LOG("init_thread_handler: Fatal error, failed to initialize enqueue mutex.");
        return false;
    }

    if(pthread_cond_init(&thread_lock_available, NULL) != 0) {
        ERROR_LOG("init_thread_handler: Fatal error, failed to initialize thread mutex condition.");
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
        // TODO: this seems wasteful, remove?
        *(threads + cursor) = (thread_instance){
            .virtual_id = cursor,
            .thread_id = (pthread_t)-1 // the actual thread's id.
        };

        if(!validate_syscall(
            pthread_create(&(threads + cursor)->thread_id, NULL, thread_runner, NULL),
            "init_thread_handler",
            "Could not create a thread for processing.")
        ) { 
            if(threads != NULL)
                free(threads);
            return false; 
        }

        if((threads + cursor)->thread_id == (pthread_t)-1) {
            ERROR_LOG("init_thread_handler: Fatal error, failed to create thread.");
            if(threads != NULL)
                free(threads);
            return false; 
        }
    }
 
    // initialize queue of connections.
    queue_head = (connection_instance *)calloc(1, sizeof(connection_instance));
    if(queue_head == NULL) {
        ERROR_LOG("init_thread_handler: Failed to allocate memory for queue.");
        if(threads != NULL)
            free(threads);
        return false;
    }

    *queue_head = (connection_instance){ 0 };
    queue_tail = &queue_head;
 
    return true;
}

bool get_request_headers(char *buffer, int socket_descriptor, int32_t *total_bytes_read) {
    char header_buffer[MAX_HEADER_SIZE] = { 0 }; 
    // header values.
    // end of http header is signalled by double new carriage.
    while(!memmem(buffer, *total_bytes_read, END_OF_BUFFER, END_OF_BUFFER_LENGTH)) {
        size_t num_bytes_read = 0;
        memset(header_buffer, 0, MAX_HEADER_SIZE); // in loop clear.
        if(!receive_data(socket_descriptor, 0, (MAX_HEADER_SIZE - 1), header_buffer, &num_bytes_read)) {
            ERROR_LOG("parse_header: Failed to receive data on thread %lu.", (unsigned long)pthread_self());
            free(buffer);
            return false;
        }

        if((*total_bytes_read + num_bytes_read) > MAX_HEADER_SIZE) {
            ERROR_LOG("parse_header: Failure, request header exceeded maximum memory capacity of %d on thread %lu.", MAX_HEADER_SIZE, (unsigned long)pthread_self());
            free(buffer);
            return false;
        }

        // end of data reached with no proper header sentinel.
        if(num_bytes_read < 1) {
            DEBUG_LOG("parse_header: Connection was closed on thread %lu.", (unsigned long)pthread_self());
            free(buffer);
            return false;
        }

        memcpy((buffer + *total_bytes_read), header_buffer, num_bytes_read);
        *total_bytes_read += num_bytes_read;
    }

    DEBUG_LOG("parse_header: Returning parsed header, value: %s.", buffer);

    return true;
}

static bool get_request_contents(char *buffer, int socket_descriptor, char *content_length_header, int32_t *total_bytes_read) {
    errno = 0;
    size_t content_length = strtoul((content_length_header + CONTENT_LENGTH_HEADER_LENGTH), NULL, 10);
    if(errno != 0) {
        ERROR_LOG("process_request: Invalid data, content length could not be parsed on thread %lu. Error: %s", (unsigned long)pthread_self(), strerror(errno));
        free(buffer);
        return false;
    }

    if(content_length > RECEIVE_BUFFER_SIZE) {
        ERROR_LOG("process_request: Failure, content length was larger than allowed buffer size on thread %lu.", (unsigned long)pthread_self());
        free(buffer);
        return false;
    }

    // body contents.
    // TODO: make the buffer dynamic so the max ingest size can be significantly larger.
    if(content_length > 0) {
        char body_buffer[RECEIVE_BUFFER_SIZE] = { 0 };
        int receive_buffer_size = RECEIVE_BUFFER_SIZE;
        size_t body_bytes_read = 0;
        while(body_bytes_read < content_length) {
            size_t num_bytes_read = 0;
            memset(body_buffer, 0, RECEIVE_BUFFER_SIZE); // in loop clear.

            if((content_length - body_bytes_read) < receive_buffer_size)
                receive_buffer_size = content_length - body_bytes_read;

            if(!receive_data(socket_descriptor, 0, (receive_buffer_size), body_buffer, &num_bytes_read)) {
                ERROR_LOG("process_request: Failed to receive data on thread %lu.", (unsigned long)pthread_self());
                free(buffer);
                return false;
            }

            DEBUG_LOG("Read %zu bytes on thread %lu.", num_bytes_read, (unsigned long)pthread_self());

            // end of data.
            if(num_bytes_read < 1) {
                DEBUG_LOG("process_request: Connection was closed on thread %lu.", (unsigned long)pthread_self());
                break;
            }

            memcpy((buffer + *total_bytes_read), body_buffer, num_bytes_read);
            total_bytes_read += num_bytes_read;
            body_bytes_read += num_bytes_read;
        }
    }

    return true;
}

