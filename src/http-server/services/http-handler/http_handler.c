#include <sys/socket.h>
#include <stdbool.h>
#include <stdlib.h>

#include "services/logging/logging.h"

#include "./http_handler.h"

// break request into string per line array.
static bool get_metadata(char *message, size_t message_size, http_request *result) {
    if(message == NULL || result == NULL) {
        ERROR_LOG("process_http_request: Parameter provided was null.");
        return false;
    }

    char current_line[MAX_HTTP_HEADER_SIZE];
    char *message_cursor = message;
    size_t num_parsed = 0;
    bool is_malformed = true;

    // get first line (type metadata).
    while(message_cursor != NULL 
        && *message_cursor != '\n' // end of line, conditional validation within block is reliant on this.
        && num_parsed < MAX_HTTP_HEADER_SIZE 
    ) {
        if(*message_cursor == '\r' && is_malformed == true) {
            is_malformed = false;
            continue;
        } else if(is_malformed == false) {
            // if statement catches for newline, '\r' is invalid in http otherwise.
            // TODO: setup a response service for bad requests.
            ERROR_LOG("process_http_request: Provided HTTP request line was malformed.");
            return false;
        }

        current_line[num_parsed] = *message_cursor;
        ++num_parsed;
        ++message_cursor;
    }

    return true;
}

bool process_http_request(int socket_descriptor, char *message, size_t message_size, char **response) {
    char *validated_request = calloc(1, sizeof(message_size));
    if(validated_request == NULL) {
       ERROR_LOG("process_http_request: Failed to allocate memory for parsed_request.");
       return false;
    }

    if(!parse_request(message, &parsed_request)) {
        ERROR_LOG("process_http_request: Failed to parse http request.");
        return false;
    }

    *response = "ok...";
    return true;
}

