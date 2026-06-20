#ifndef THREAD_HANDLER_H
#define THREAD_HANDLER_H

#define RECEIVE_BUFFER_SIZE 16384 // single ram page size.
#define NUM_CONNECTIONS 32768

bool init_thread_handler();

bool enqueue_task(int client_descriptor);

bool pull_next_task(int *result);

#endif
