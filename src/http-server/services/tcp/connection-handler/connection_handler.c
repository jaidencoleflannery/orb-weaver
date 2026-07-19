#include <sys/socket.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "types/address-types/address_types.h"
#include "services/logging/logging.h"
#include "configuration/parsers/configuration-handler/configuration_handler.h"
#include "utilities/error-handler/error_handler.h"

#include "./connection_handler.h"

static int socket_descriptor = -1;
static addrinfo *bound_address = NULL;

bool get_socket_descriptor(int *result) {
    if(socket_descriptor < 0) {
        ERROR_LOG("get_socket_descriptor: socket_descriptor was invalid.");
        return false;
    }

    *result = socket_descriptor;
    return true;
}

bool find_bind(addrinfo *addresses, int *socket_descriptor) {
    const size_t port = config.port;

    while(addresses != NULL) { 
        if((*socket_descriptor = socket(addresses->ai_family, SOCK_STREAM, 0)) < 0) {
            char *socket_error = strerror(errno);
            DEBUG_LOG("find_bind: Failed to open a socket, error: %s.", socket_error);
            addresses = addresses->ai_next;
            continue;
        }

        setsockopt(*socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

        int status;
        if((status = bind(*socket_descriptor, addresses->ai_addr, addresses->ai_addrlen)) < 0) {
            char *bind_error = strerror(errno);
            DEBUG_LOG("find_bind: Failed to bind to local port, closing file descriptor. Error: %s.", bind_error);
            close(*socket_descriptor);
            *socket_descriptor = -1;
            addresses = addresses->ai_next;
            continue;
        }

        // persist past stack frame.
        bound_address = addresses;
        break;
    }

    if(*socket_descriptor < 0) {
        DEBUG_LOG("find_bind: No successful bindings for provided addresses.");
        return false;
    }

    DEBUG_LOG("find_bind: Bound successfully. File descriptor: %d.", *socket_descriptor);
    return true;
}

// for communicating with external servers.
// TODO: this function needs to be fixed.
bool find_connection(addrinfo *addresses) {
    // iterate, first fit algo.  
    if(!find_bind(addresses, &socket_descriptor)) {
        DEBUG_LOG("find_connection: Failed to connect to any provided addresses.\n");
        return false;
    }

    if(connect(socket_descriptor, (sockaddr *)bound_address, sizeof(sockaddr_in)) < 0) {
        char *connect_error = strerror(errno);
        DEBUG_LOG("find_connection: Failed to bind to local port, error: %s.\n", connect_error);
        close(socket_descriptor);
        return false;
    }

    freeaddrinfo(addresses);
    return true;
}

bool find_listen(addrinfo *addresses, addrinfo *bound_address) {
    // first fit algo.
    if(!find_bind(addresses, &socket_descriptor)) {
        ERROR_LOG("find_connection: Failed to bind to any provided addresses.");
        return false;
    }

    if(!validate_syscall(
        listen(socket_descriptor, (int)config.max_connections),
        "find_listen", 
        "Failed to listen to local port.")
    ) { return false; }

    DEBUG_LOG("find_listen: Listening successfully.");
    return true;
}

bool shutdown_connection(int file_descriptor, int type) {
    int shutdown_return = shutdown(file_descriptor, type);
    if(!validate_syscall(shutdown_return, "shutdown_connection", "Error encountered while attempting to close socket."))
        return false;

    return true;
}

bool accept_connection(sockaddr_storage *address, int *client_descriptor) {
    DEBUG_LOG("accept_connection: Attempting to accept connection.");

    *client_descriptor = accept(socket_descriptor, (sockaddr *)address, &(socklen_t){ sizeof(sockaddr_storage) });
    if(!validate_syscall(*client_descriptor, "accept_connection", "Listening file descriptor did not give any data.")) {
        DEBUG_LOG("accept_connection: Error, client_descriptor was set to: %d.", *client_descriptor);
        *client_descriptor = -1;
        return false;
    }

    DEBUG_LOG("accept_connection: Successfully accepted connection for %d.", *client_descriptor);
    return true;
}

bool close_connection(int *client_descriptor) {
    DEBUG_LOG("close_connection: Attempting to close connection %d.", *client_descriptor);

    int close_status = close(socket_descriptor);
    if(!validate_syscall(*client_descriptor, "close_connection", "Listening file descriptor did not give any data.")) {
        DEBUG_LOG("close_connection: Error, unable to close connection for: %d.", *client_descriptor);
        return false;
    }

    DEBUG_LOG("close_connection: Successfully closed connection for %d.", *client_descriptor);
    return true;
}

bool send_data(int file_descriptor, char *data, size_t data_length, int flags) {
    size_t total_bytes_sent = 0;
    while(total_bytes_sent < data_length) {
        size_t bytes_sent = send(file_descriptor, (data + bytes_sent), (data_length - bytes_sent), 0);
        if(!validate_syscall(bytes_sent, "send_data", "Failed to send data."))
            return false;
        if(bytes_sent == 0) {
            ERROR_LOG("send_data: Unexpected error occurred while attempting to send data.\n");
            return false;
        }

        total_bytes_sent += bytes_sent;
        bytes_sent = 0; 
    }

    return true;
}

bool receive_data(int file_descriptor, int flags, size_t buffer_length, char *buffer, size_t *num_bytes_read) {
    ssize_t bytes_received = recv(file_descriptor, buffer, buffer_length, flags);
    if(!validate_syscall(bytes_received, "receive_data", "Failed to receive data."))
        return false;

    *num_bytes_read = (size_t)bytes_received;

    if(bytes_received == 0) {
        DEBUG_LOG("receive_data: Remote connection was closed.\n");
        return true; 
    }

    return true;
}

bool get_peer_name(int file_descriptor, sockaddr *result) {
    int peer_return = getpeername(file_descriptor, result, &(socklen_t){ sizeof(sockaddr) });
    if(!validate_syscall(peer_return, "get_peer_name", "Failed to get peer name."))
        return false;

    return true;
}

bool get_host_name(char *result, size_t result_size) {
    int host_return = gethostname(result, result_size);
    if(!validate_syscall(host_return, "get_host_name", "Failed to get host name."))
        return false;

    return true;
}

