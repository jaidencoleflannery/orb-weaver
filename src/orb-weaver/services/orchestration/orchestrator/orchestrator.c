#include <sys/event.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netdb.h>

#include "types/address-types/address_types.h"
#include "services/logging/logging.h"
#include "services/orchestration/thread-handler/thread_handler.h"
#include "services/tcp/host-resolver/host_resolver.h"
#include "services/tcp/connection-handler/connection_handler.h"
#include "configuration/parsers/configuration-handler/configuration_handler.h"
#include "utilities/error-handler/error_handler.h"

#include "./orchestrator.h"

static bool process_connection_event();
static bool process_data_event(struct kevent *event);
static bool enable_event(unsigned long *socket_descriptor);

/*
 * orchestrator adds to a task queue via a multiplex cycle. 
 * the queue is then processed fifo by thread_handler.
*/

static int event_queue;

// + add new connections to kqueue.
// + listen for data events on existing connections and process.
static bool poll_events(int socket_descriptor) {
    // server runtime.
    while(1) {
        struct kevent events[MAX_EVENTS];
        // blocks for events on main socket + any connections (sockets) we subscribe to.
        int num_events = kevent(event_queue, NULL, 0, events, MAX_EVENTS, NULL);
        if(!validate_syscall(
            num_events,
            "poll_events",
            "Fatal failure, failed to poll for events.")
        ) { return false; }

        if(num_events > 0) {
            // process all new events on each poll cycle.
            for(int cursor = 0; cursor < num_events; cursor++) {
                struct kevent event = *(events + cursor);
                DEBUG_LOG("poll_events: Event found."); 
     
                if((int)event.ident == socket_descriptor) {
                    // new connection.
                    if(!process_connection_event())
                        continue;

                } else {
                    // connection received data, add to task queue so a thread can process.
                    if(!process_data_event(&event))
                        continue;
                }
            }
        }
    }
}

// + config initialization.
// + listen to main socket.
bool boot_server(void) {
    LOG("[ ORB ]", "Booting server.");

    if(!initialize_configuration()) {
        ERROR_LOG("boot_server: Failed to initialize configuration.");
        return false;
    }
 
    // fill struct with local address information.
    addrinfo *addresses = { 0 };
    if(!get_local_addresses(false, &addresses)) {
        ERROR_LOG("boot_server: Failed to fetch local addresses.");
        return false;
    } 

    // bind and listen.
    addrinfo bound_address;
    if(!find_listen(addresses, &bound_address)) {
        ERROR_LOG("boot_server: Failed to listen to local address.");
        return false;
    }

    freeaddrinfo(addresses);

    LOG("[ ORB ]", "Listening on port: %zu.", config.port);
    return true;
}

// + thread initialization.
// + listen for new connections to main socket. 
bool start_processing(void) {
    // fd is cached from socket init.
    int socket_descriptor = -1;
    if(!get_socket_descriptor(&socket_descriptor) || socket_descriptor < 0) {
        ERROR_LOG("start_processing: Fatal error, failed to fetch socket_descriptor. Descriptor returned: %d.", socket_descriptor);
        return false;
    }

    // shamefully greedy process lets the thread pool sit.
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

    // subscribe to main socket for connection events.
    struct kevent data_event;
    EV_SET(&data_event, socket_descriptor, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if(!validate_syscall(
        kevent(event_queue, &data_event, 1, NULL, 0, NULL),
        "start_processing",
        "Fatal error, failed to create kevent for main connection socket.")
    ) { return false; } 

    // server runtime.
    if(!poll_events(socket_descriptor)) {
        ERROR_LOG("start_processing: Encountered an error invoking poll_events.");
        return false;
    }

    return true;
}


/*
 * + accepts new connections.
 * + adds new connection to kqueue for watching.
*/
static bool process_connection_event(void) {
    DEBUG_LOG("process_connection_event: New connection received.");

    sockaddr_storage *client_address = &(sockaddr_storage){ 0 }; 

    int client_descriptor = -1;
    if(!accept_connection(client_address, &client_descriptor)) {
        ERROR_LOG("process_connection_event: Failed to accept connection.");
        return false;
    }

    if(client_descriptor == -1) {
        // if we fail to accept the connection, ignore the request; this should never happen.
        // no need to close connection here, it was never instantiated.
        ERROR_LOG("process_connection_event: Error occured attempting to accept connection, client_descriptor was not properly set when the connection was accepted. Tossing connection attempt."); 
        return false;
    }

    // add new connection to queue to watch for future data.
    struct kevent client_event;
    // TODO: add metadata to event.ident here for future use.
    // event.ident 
    EV_SET(&client_event, client_descriptor, EVFILT_READ, EV_ADD | EV_DISPATCH, 0, 0, NULL);
    if(!validate_syscall(
        kevent(event_queue, &client_event, 1, NULL, 0, NULL),
        "process_connection_event",
        "Failed to add event for new connection.")
    ) { return false; }

    return true;
}

/*
 * + processes data received from existing connections.
 * + adds new connection to kqueue for watching.
*/
static bool process_data_event(struct kevent *event) {
    DEBUG_LOG("process_data_event: Data received."); 

    uintptr_t current_descriptor = event->ident;
    if(current_descriptor < 0) {
        ERROR_LOG("poll_events: Fatal error, unable to fetch socket ID for connection to client.");
        return false;
    } 

    // connection dropped.
    if(event->flags & EV_EOF) { 
        DEBUG_LOG("poll_events: Dropped connection event.");

        struct kevent client_event;
        EV_SET(&client_event, event->ident, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        validate_syscall(
            kevent(event_queue, &client_event, 1, NULL, 0, NULL),
            "poll_events",
            "Failed to delete subscription from kqueue."
        );

        if(!validate_syscall(event->ident, "pull_events", "Failed to close connection.")) {
            DEBUG_LOG("close_connection: Error, unable to close connection for: %lu.", event->ident);
            return false;
        };
    }

    DEBUG_LOG("poll_events: Enqueueing task %lu.", current_descriptor);

    if(!enqueue_task(current_descriptor)) {
        ERROR_LOG("poll_events: Fatal error, unable to add connection to thread queue.");
        return false;
    }

    return true;
}

// + reenable event after processing.
// this needs to be called from within the handler thread.
static bool enable_event(unsigned long *socket_descriptor) {
    struct kevent change;
    struct kevent receipt;

    EV_SET(&change, *socket_descriptor, EVFILT_READ, EV_ENABLE | EV_RECEIPT, 0, 0, NULL);

    int result = kevent(event_queue, &change, 1, &receipt, 1, NULL);
    if(!validate_syscall(
        result,
        "enable_event",
        "Failed to enable event on multiplexer."
    )) { return false; }

    if(!validate_syscall(
        receipt.data,
        "enable_event",
        "Failed to enable event on multiplexer, receipt suggests an error occured."
    )) { return false; }

    return true;
}

