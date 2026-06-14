#include <sys/socket.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "services/logging/logging.h"

#include "./http_handler.h"

static bool validate_http_type(char *type, size_t type_size) {
    if(type == NULL) {
        ERROR_LOG("validate_http_type: Provided type parameter was invalid.");
        return false;
    }

    return true;
}

static bool validate_http_headers(char *type, size_t type_size) {
    if(type == NULL) {
        ERROR_LOG("validate_http_type: Provided type parameter was invalid.");
        return false;
    }

    return true;
}

// request metadata (line 0) is considered a header here.
static bool get_http_metadata(char *message, size_t message_size, http_request *result) {
    if(message == NULL || result == NULL || message_size == 0) {
        ERROR_LOG("get_http_metadata: Parameter provided was invalid.");
        return false;
    }

    char header_line[MAX_HTTP_HEADER_SIZE];
    size_t line_increment = 0; // to validate entry in type header line.
    char *message_cursor = message;
    size_t num_parsed = 0;
    bool is_malformed = true;

    // get first line (type metadata).
    // '\n' denotes end of line, conditional validation within block is reliant on this.
    while(message_cursor != NULL && *message_cursor != '\n') {
        if(num_parsed == MAX_HTTP_HEADER_SIZE ) {
            ERROR_LOG("get_http_metadata: Provided HTTP request's header value exceeded the maximum length of %d.", MAX_HTTP_HEADER_SIZE);
            return false;
        } else if(is_malformed == false) {
            // TODO: setup a response service for bad requests.
            ERROR_LOG("process_http_request: Provided HTTP request line was malformed.");
            return false;
        } else if(*message_cursor == '\r') {
            // '\r' marks the new carriage and is only valid for the end of a line (ie header value).
            // '\r' must be followed by '\n'.
            is_malformed = false;
            continue;
        }

        if(*message_cursor == ' ') {
            bool validation_result = false;
            // if first line, get type.
            if(line_increment < 1)
                validation_result = validate_http_type(header_line, num_parsed);
            else
                validation_result = validate_http_headers(header_line, num_parsed);

            if(!validation_result) {
                ERROR_LOG("get_http_metadata: Provided HTTP request's headers vere invalid.");
                return false;
            }

            // clear values so next iteration can grab headers.
            memset(header_line, 0, sizeof(header_line));
        }

        header_line[num_parsed] = *message_cursor;
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

