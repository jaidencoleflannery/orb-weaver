#ifndef THREAD_HANDLER_H
#define THREAD_HANDLER_H

#define RECEIVE_BUFFER_SIZE 32768
#define MAX_HEADER_SIZE 8192
#define NUM_CONNECTIONS 32768
#define MAX_RESPONSE_SIZE 32768

// lengths are for comparison and do not include the null terminator.
#define END_OF_BUFFER "\r\n\r\n"
#define END_OF_BUFFER_LENGTH 4
#define CONTENT_LENGTH_HEADER "Content-Length:"
#define CONTENT_LENGTH_HEADER_LENGTH 15

bool init_thread_handler();

bool enqueue_task(uintptr_t client_descriptor);

#endif
