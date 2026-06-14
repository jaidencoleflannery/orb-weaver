#include <sys/event.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netdb.h>

#include "types/address-types/address_types.h"
#include "services/logging/logging.h"
#include "services/thread-handler/thread_handler.h"
#include "services/host-resolver/host_resolver.h"
#include "services/connection-handler/connection_handler.h"
#include "configuration/configuration-handler/configuration_handler.h"
#include "utilities/error-handler/error_handler.h"

#include "./orchestrator.h"

/*
 * orchestrator adds to a task queue via a multiplex cycle. 
 * the queue is then processed fifo by thread_handler.
 */

static int event_queue;

static bool poll_events(int socket_descriptor) {
    // server runtime. 
    while(1) {
        struct kevent event;
        // check for events.
        int num_events = kevent(event_queue, NULL, 0, &event, 1, NULL);
        if(!validate_syscall(
            num_events,
            "poll_events",
            "Fatal failure, failed to poll for events.")
        ) { return false; }

        if(num_events > 0) {
            DEBUG_LOG("poll_events: Connection event flag signalled.");

            sockaddr_storage *client_address = &(sockaddr_storage){ 0 };
            int *client_descriptor = calloc(1, sizeof(int));
            if(client_descriptor == NULL) {
                ERROR_LOG("poll_events: Failed to allocate memory for client_descriptor.");
                return false;
            }

            *client_descriptor = -1; 
 
            if((int)event.ident == socket_descriptor) {
                // new connection.
                DEBUG_LOG("poll_events: New connection event."); 

                if(!accept_connection(client_address, client_descriptor)) {
                    ERROR_LOG("poll_events: Failed to accept connection.");
                    continue;
                }

                if(*client_descriptor == -1) {
                    ERROR_LOG("poll_events: The stack variable client_descriptor was not properly set when the connection was accepted.");
                    continue; // skip the connection; this should never happen.
                }

                // add new connection to queue.
                struct kevent client_event;
                EV_SET(&client_event, socket_descriptor, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, &socket_descriptor);
                if(!validate_syscall(
                    kevent(event_queue, &client_event, 1, NULL, 0, NULL),
                    "poll_events",
                    "Failed to add event for new connection.")
                ) { continue; }

            } else if(event.flags & EV_EOF) {
                // connection dropped.
                DEBUG_LOG("poll_events: Dropped connection event.");

                struct kevent client_event;
                EV_SET(&client_event, socket_descriptor, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                if(!validate_syscall(
                    kevent(event_queue, &client_event, 1, NULL, 0, NULL),
                    "poll_events",
                    "Failed to delete subscription from kqueue.")
                ) { continue; }

            } else {
                // connection received data, invoke a thread to process.
                DEBUG_LOG("poll_events: Data received event.");

                int socket_descriptor = *(int *)event.udata;
                if(socket_descriptor < 0) {
                    ERROR_LOG("poll_events: Fatal error, unable to fetch socket ID for connection to client.");
                    return false;
                }

                if(!enqueue_task(socket_descriptor)) {
                    ERROR_LOG("poll_events: Fatal error, unable to add connection to thread queue.");
                    return false;
                } 
            }
        }
    }
}

bool boot_server() {
    LOG("[ ORB ]", "Booting server.");

    if(!initialize_configuration()) {
        ERROR_LOG("boot_server: Failed to initialize configuration.");
        return false;
    }
 
    addrinfo *addresses = { 0 };
    if(!get_local_addresses(false, &addresses)) {
        ERROR_LOG("boot_server: Failed to fetch local addresses.");
        return false;
    } 

    addrinfo bound_address; 
    if(!find_listen(addresses, &bound_address)) {
        ERROR_LOG("boot_server: Failed to listen to local address.");
        return false;
    }

    freeaddrinfo(addresses);

    LOG("[ ORB ]", "Listening on port: %zu.", config.port);
    return true;
}

bool start_processing() {
    // initialize data.
    int socket_descriptor = -1;
    if(!get_socket_descriptor(&socket_descriptor) || socket_descriptor < 0) {
        ERROR_LOG("start_processing: Fatal error, failed to fetch socket_descriptor. Descriptor returned: %d.", socket_descriptor);
        return false;
    }

    if(!init_thread_handler()) {
        ERROR_LOG("start_processing: Fatal error, failed to initialize thread handler.");
        return false;
    }
 
    event_queue = kqueue();
    if(!validate_syscall(
        event_queue,
        "start_processing",
        "Fatal error, Failed to initialize kqueue.")
    ) { return false; }  

    // subscribe to socket for update events.
    struct kevent data_event;
    EV_SET(&data_event, socket_descriptor, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if(!validate_syscall(
        kevent(event_queue, &data_event, 1, NULL, 0, NULL),
        "start_processing",
        "Fatal error, failed to create kevent for main connection socket.")
    ) { return false; } 

    if(!poll_events(socket_descriptor)) {
        ERROR_LOG("start_processig: Encountered an error invoking poll_events.");
        return false;
    }

    return true;
}

