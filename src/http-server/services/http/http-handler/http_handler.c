#include <sys/socket.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "services/logging/logging.h"
#include "types/http-types/http_types.h"

#include "./http_handler.h"

// method validation is highly dependent on the http_request_method enum.
// this validates the entire first line of the http request.
static bool validate_http_metadata(char *line, size_t line_size, http_request *result_metadata) {
    if(line == NULL) {
        ERROR_LOG("validate_http_method: Provided method parameter was invalid.");
        return false;
    }

    char line_partition[line_size];

    size_t num_parsed       = 0;
    size_t word_increment   = 0;

    size_t method_size      = 0;
    size_t route_size       = 0;
    size_t protocol_size     = 0;

    char *line_cursor = line;
    while(line_cursor != NULL) {
        if(num_parsed >= line_size || num_parsed >= MAX_HTTP_HEADER_SIZE) {
            ERROR_LOG("validate_http_method: Provided line exceeded the maximum header size.");
            return false;
        }

        // the cache for the string we're checking
        // is filled at the bottom of this function.
        if(*line_cursor == ' ') {
            if(word_increment == 0) { // request method.
                if(method_size < 3) {
                    ERROR_LOG("validate_http_method: First value (method) of the method line was invalid.");
                    return false;
                }

                bool method_valid = false;
                for(int method_cursor = 0; method_cursor < TYPE_COUNT; method_cursor++) {
                    if(strcmp(line_partition, method_entries[method_cursor].name) == 0) {
                        result_metadata->http_method = method_entries[method_cursor].method;
                        method_valid = true;
                        break;
                    }
                }

                if(!method_valid) {
                    ERROR_LOG("validate_http_method: First value (method) of the method line was invalid.");
                    result_metadata->http_method = TYPE_NULL;
                    return false;
                }  
            } else if(word_increment == 1) { // request route.
                if(route_size < 1) {
                    ERROR_LOG("validate_http_method: Second value (route) of the method line was invalid.");
                    return false;
                }

                bool route_valid = false;
                // if you're looking for the route handler, you're at the wrong castle.
                if(*line_partition == '/') {
                    memcpy(result_metadata->http_route, line_partition, strlen(line_partition) + 1); // + 1 for '\0'.
                    result_metadata->http_route_size = route_size;
                    route_valid = true;
                }

                if(!route_valid) {
                    ERROR_LOG("validate_http_method: Second value (route) of the method line was invalid.");
                    result_metadata->http_route = NULL;
                    return false;
                }
            } else if(word_increment == 2) { // protocol.
                if(protocol_size < 1) {
                    ERROR_LOG("validate_http_method: Third value (protocol) of the method line was malformed.");
                    return false;
                }

                bool protocol_valid = false;

                for(int protocol_cursor = 0; protocol_cursor < VERSION_COUNT; protocol_cursor++) {
                    if(strcmp(line_partition, protocol_entries[protocol_cursor].name) == 0) {
                        result_metadata->http_protocol = protocol_entries[protocol_cursor].protocol;
                        protocol_valid = true;
                        break;
                    }
                }

                if(strcmp(line_partition, PROTOCOL) == 0)
                    protocol_valid = true;

                if(!protocol_valid) {
                    ERROR_LOG("validate_http_method: Third value (protocol) of the method line was invalid.");
                    return false;
                }

                // request line 0 is minimally valid.
                return true;
            } 

            ++word_increment;
            ++line_cursor;
            ++num_parsed;
            line_cursor = (line + num_parsed);
            // clear for next partition.
            memset(line_partition, 0, line_size);
            continue;
        }

        switch(word_increment) {

            case 0: // method.
                line_partition[method_size++] = *line_cursor;
                break;

            case 1: // route.
                line_partition[route_size++] = *line_cursor;
                break;

            case 2: // protocol
                line_partition[protocol_size++] = *line_cursor;
                break;

        }

        ++num_parsed;
        line_cursor = (line + num_parsed); 
    }

    return true;
}

// validate a single header.
static bool validate_http_header(char *header, size_t header_size, http_request *result_metadata) {
    if(header == NULL) {
        ERROR_LOG("validate_http_header: Provided header parameter was invalid.");
        return false;
    }

    // storage for parsed header.
    http_request_header *result = (http_request_header *)calloc(1, sizeof(http_request_header)); 
    if(result == NULL) {
        ERROR_LOG("validate_http_header: Failed to allocate memory for header result.");
        return false;
    } 

    result->value = (char *)calloc(1, sizeof(header_size));
    if(result->key == NULL) {
        ERROR_LOG("validate_http_header: Failed to allocate memory for header result value.");
        return false;
    }

    char key[header_size];
    char value[header_size];
    int num_parsed;

    // flag for three portions of header.
    header_status flag = IS_KEY;

    // flag for end of line.
    bool end_flag = false;

    char *header_cursor = header;
    while(header_cursor != NULL) {  
        // end of key sentinel, set status flags.
        if(*header_cursor == ':') { 
            if(flag != IS_KEY) {
                ERROR_LOG("validate_http_header: Header was malformed.");
                return false;
            }

            flag = IS_ASSIGNING; // next char needs to be a space.
            ++header_cursor;
            continue;
        }

        // end of key reached, if syntax is correct store the value and clear cache.
        if(*header_cursor == ' ') { 
            if(flag != IS_ASSIGNING) {
                ERROR_LOG("validate_http_header: Header was malformed.");
                return false;
            }        

            result->key = calloc(1, num_parsed);
            if(result->key == NULL) {
                ERROR_LOG("validate_http_header: Failed to allocate memory for header result key.");
                return false;
            }

            memcpy(result->key, key, num_parsed);

            flag = IS_VALUE;
            ++header_cursor;
            num_parsed = 0;
            continue;
        }

        // last character was ':' but this character was not ' '.
        if(flag == IS_ASSIGNING && *header_cursor != ' ') {
            ERROR_LOG("validate_http_header: Header was malformed.");
            return false;
        }

        if(*header_cursor == '\r') {
            if(flag != IS_VALUE) {
                ERROR_LOG("validate_http_header: Header was malformed.");
                return false;
            }

            end_flag = true;
            // TODO: validate end of header.
        }

        // assign values if we pass the validations.
        if(flag == IS_KEY)
            memcpy(key, header_cursor, num_parsed);
        else if(flag == IS_VALUE)
            memcpy(value, header_cursor, num_parsed);

        ++header_cursor;
        ++num_parsed;
    }

    return true;
}

// request metadata (line 0) is considered a header here.
static bool get_http_metadata(char *message, size_t message_size, http_request **result_metadata) {
    if(message == NULL 
    || result_metadata == NULL 
    || *result_metadata == NULL
    || message_size == 0) {
        ERROR_LOG("get_http_metadata: Parameter provided was invalid.");
        return false;
    }

    char *message_cursor = message;
    bool end_flag = false;

    char header_line[MAX_HTTP_HEADER_SIZE];
    size_t line_increment = 0; // to validate entry in method header line. 
    size_t num_parsed = 0; 

    // get first line (method metadata).
    // '\r\n' denotes end of line, conditional validation within block is reliant on this.
    while(message_cursor != NULL) {
        if(num_parsed == MAX_HTTP_HEADER_SIZE ) {
            ERROR_LOG("get_http_metadata: Provided HTTP request's header value exceeded the maximum length of %d.", MAX_HTTP_HEADER_SIZE);
            return false;
        }

        if(*message_cursor == '\r') {
            // '\r' marks the new carriage and is only valid for the end of a line (ie header value).
            // '\r' must be followed by '\n'.
            end_flag = true;
            ++message_cursor;
            continue;
        }

        // end of line logic.
        if((*message_cursor == '\n' && end_flag != true) 
        || (*message_cursor != '\n' && end_flag == true)) {
            // TODO: setup a path for bad requests.
            ERROR_LOG("get_http_metadata: Provided HTTP request line was malformed.");
            return false; 
        } else if(*message_cursor == '\n' && end_flag == true) {
            bool validation_result = false;
            if(line_increment < 1) // if first line, get method.
                validation_result = validate_http_metadata(header_line, num_parsed, *result_metadata); 
            else // else, parse current header.
                validation_result = validate_http_header(header_line, num_parsed, *result_metadata);

            if(!validation_result) {
                ERROR_LOG("get_http_metadata: Provided HTTP request's method or headers vere invalid.");
                return false;
            }

            // clear values so next iteration is clean.
            memset(header_line, 0, sizeof(header_line));
            num_parsed = 0;
            ++line_increment;
        }

        // actual parsing.
        header_line[num_parsed] = *message_cursor;
        ++num_parsed;
        ++message_cursor;

        DEBUG_LOG("get_http_metadata: looped.");
    }

    return true;
}

static bool route_http_request(char *message, size_t message_size, http_request **result_metadata) {
    if(message == NULL 
    || result_metadata == NULL 
    || *result_metadata == NULL
    || message_size == 0) {
        ERROR_LOG("get_http_metadata: Parameter provided was invalid.");
        return false;
    }

    DEBUG_LOG("route_http_request: Reached end of current implementation.");

    // TODO: complete this once dynamic routing has been implemented.
    return true;
}

// orchestrator for http request handling.
bool process_http_request(int socket_descriptor, char *message, size_t message_size, char **response) {
    if(message == NULL || response == NULL) {
        ERROR_LOG("process_http_request: Invalid parameter was provided.");
        return false;
    }

    // heap alloc.
    http_request *parsing_result;
    if(!allocate_http_request(&parsing_result)) {
        ERROR_LOG("process_http_request: Failed to allocate memory for http_request.");
        return false;
    }

    if(!get_http_metadata(message, message_size, &parsing_result)) {
        ERROR_LOG("process_http_request: Failed to parse http request headers.");
        return false;
    }

    if(!route_http_request(message, message_size, &parsing_result)) {
        ERROR_LOG("process_http_request: Failed to route HTTP request.");
        return false;
    }

    // free memory.
    if(!free_http_request(parsing_result)) {
        ERROR_LOG("process_http_request: Failed to free instance of http_request.");
        return false;
    }

    // TODO: memcopy response into response parameter.

    return true;
}

