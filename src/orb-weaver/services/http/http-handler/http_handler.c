#include <sys/socket.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "services/logging/logging.h"
#include "types/http-types/http_types.h"

#include "./http_handler.h"

bool _clean_header(http_request_header **header_instance);

static bool _validate_http_metadata(char *line, size_t line_size, http_request *result_metadata);
static bool _validate_http_header(char *header, size_t header_size, bool *host_found, http_request_header **result_header); 
static bool _get_http_metadata(char *raw, size_t raw_size, http_request **result_metadata);
static bool _route_http_request(char *message, size_t message_size, http_request **result_metadata);

// orchestrator for http request handling.
bool process_http_request(int socket_descriptor, char *message, size_t message_size, http_request **response) {
    if(message == NULL || response == NULL) {
        ERROR_LOG("process_http_request: Invalid parameter was provided.");
        return false;
    }

    // heap alloc.
    http_request *parsing_result = { 0 };
    if(!allocate_http_request(&parsing_result)) {
        ERROR_LOG("process_http_request: Failed to allocate memory for http_request.");
        return false;
    }

    // get, validate and store header values.
    if(!_get_http_metadata(message, message_size, &parsing_result)) {
        ERROR_LOG("process_http_request: Failed to parse http request headers.");
        return false;
    }

    if(!_route_http_request(message, message_size, &parsing_result)) {
        ERROR_LOG("process_http_request: Failed to route HTTP request.");
        return false;
    }

    // free memory.
    if(!free_http_request(parsing_result)) {
        ERROR_LOG("process_http_request: Failed to free instance of http_request.");
        return false;
    }

    // TODO: process routing and return value in response parameter.

    return true;
}

// method validation is highly dependent on the http_request_method enum.
// this validates the entire first line of the http request.
static bool _validate_http_metadata(char *line, size_t line_size, http_request *result_metadata) {
    if(line == NULL) {
        ERROR_LOG("validate_http_method: Provided method parameter was invalid.");
        return false;
    }

    char line_partition[line_size];

    size_t num_parsed = 0;
    size_t word_increment = 0;

    size_t method_size = 0;
    size_t route_size = 0;
    size_t protocol_size = 0;

    char *line_cursor = line;
    while(line_cursor != NULL) {
        if(num_parsed > line_size || num_parsed > MAX_HTTP_HEADER_LINE_SIZE) {
            ERROR_LOG("validate_http_method: Provided line exceeded the maximum header size.");
            return false;
        }

        // the cache for the string we're checking
        // is filled at the bottom of this function.
        if(*line_cursor == ' ' || *line_cursor == '\0') {
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
                if(word_increment >= 2
                || result_metadata->http_method == TYPE_NULL
                || result_metadata->http_route == NULL
                || result_metadata->http_route_size < 1)
                    return true;
                else
                    break; // failure fallthrough.
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

    ERROR_LOG("validate_http_method: Failure, unexpected fallthrough condition caught, header value was malformed.");
    return false;
}

// validate a single header.
static bool _validate_http_header(char *header, size_t header_size, bool *host_found, http_request_header **result_header) {
    if(header == NULL) {
        ERROR_LOG("Failed to validate header, provided header was invalid.");
        return false;
    }

    if(result_header == NULL
    || *result_header == NULL) {
        ERROR_LOG("Failed to validate header, provided result pointer was invalid.");
        return false;
    } 

    char key_cache[header_size];
    size_t key_cache_size = 0;
    memset(key_cache, 0, header_size);

    char value_cache[header_size];
    size_t value_cache_size = 0;
    memset(value_cache, 0, header_size);

    // state machine for three portions of header.
    header_status flag = IS_KEY;

    // http spec requirement validation.
    *host_found = false;

    int num_parsed = 0;
    int total_parsed = 0;
    char *header_cursor = header; 
    while(header_cursor != NULL && total_parsed <= header_size) {

        // end of key sentinel, set status flags.
        if(flag == IS_KEY 
        && *header_cursor == ':') {
            flag = IS_ASSIGNING; // next char needs to be a space.

            if(!strcmp(key_cache, HOST_HEADER_KEY))
                *host_found = true;
            
            if((header_cursor + 1)
            && *(header_cursor + 1) != ' ') {
                ERROR_LOG("Failed to parse header, malformed data was encountered.");
                return false;
            }

            flag = IS_VALUE;
            header_cursor += 2; // skip the space.
            total_parsed += 2;
            num_parsed = 0;

            continue;
        }

        // assign values if we pass the validations.
        if(flag == IS_KEY) {
            key_cache[key_cache_size] = *header_cursor;
            ++key_cache_size;
        } else if(flag == IS_VALUE) {
            value_cache[value_cache_size] = *header_cursor;
            ++value_cache_size;
        }

        if(*header_cursor == '\r') {
            if(flag != IS_VALUE) {
                ERROR_LOG("_validate_http_header: Header was malformed, encountered a newline character within the header line.");
                return false;
            }

            if((header_cursor + 1) == NULL
            || *(header_cursor + 1) != '\n') {
                ERROR_LOG("Failure, header was malformed around a newline character.");
                return false;
            }
        }

        ++header_cursor;
        ++num_parsed; 
        ++total_parsed;
    }
 
    // persist data.
    http_request_header *result = *result_header;

    // strndup is a heap allocation.
    result->key_size = key_cache_size;
    result->key = strndup(key_cache, key_cache_size); 

    result->value_size = value_cache_size;
    result->value = strndup(value_cache, value_cache_size);

    if(!result->key || !result->value) {
        ERROR_LOG("Failed to allocate and copy memory for header fields.");
        _clean_header(&result);
        return false;
    }

    result->valid = true;

    return true;
}

// request metadata (line 0) is considered a header here.
// \r\n\r\n has already been stripped out at this stage.
static bool _get_http_metadata(char *raw, size_t raw_size, http_request **result_metadata) {
    if(raw == NULL
    || result_metadata == NULL
    || *result_metadata == NULL
    || raw_size == 0
    || raw_size > (MAX_HTTP_HEADER_TOTAL_SIZE)) {
        ERROR_LOG("_get_http_metadata: Parameter provided was invalid.");
        return false;
    }

    char *raw_cursor = raw;
    char header_buffer[MAX_HTTP_HEADER_LINE_SIZE] = { 0 };
    uint32_t line_increment = 0; // to validate entry in method header line.
    size_t num_parsed = 0;

    http_request_header *validated_headers[MAX_HTTP_HEADER_COUNT];
    uint32_t validated_cursor = 0;

    while(raw_cursor != NULL) {
        if(num_parsed == MAX_HTTP_HEADER_LINE_SIZE ) {
            ERROR_LOG("_get_http_metadata: Provided HTTP request's header value exceeded the maximum length of %d.", MAX_HTTP_HEADER_LINE_SIZE);
            return false;
        }
 
        if(*raw_cursor != '\r') {

            header_buffer[num_parsed] = *raw_cursor;
            ++num_parsed;
            ++raw_cursor;

        } else { // end of line logic.
            char header_line[num_parsed + 1];
            size_t copy_counter = 0;

            // copy cache into header line.
            for(int cursor = 0; cursor < (num_parsed + 1); cursor++) {
                if((header_buffer + cursor) == NULL) {
                    ERROR_LOG("Failed to copy header value from cache, source string was malformed.");
                    return false;
                }

                char *source = header_buffer + cursor;
                char *destination = header_line + cursor;

                if((*destination = *source) == '\0')
                    break;

                ++copy_counter;
            }

            if(copy_counter != num_parsed) {
                ERROR_LOG("Failed to properly copy header value from cache (character count differs).");
                return false;
            }

            // '\r\n' marks the new carriage and is only valid for the end of a line (ie header value).
            if(*raw_cursor == '\r'
            && (raw_cursor + 1)
            && *(raw_cursor + 1) == '\n') {
                if(!(raw_cursor + 2)
                || !(raw_cursor + 3)) {
                    ERROR_LOG("Failure, header values were malformed. Section ended without a proper double new carriage partition.");
                    return false;
                } 

                if(line_increment < 1) { // if first line, get method - this stores the parsed value.
                    if(!_validate_http_metadata(header_line, num_parsed, *result_metadata)) {
                        ERROR_LOG("HTTP metadata within request was invalid, denying request.");
                        return false;
                    }
                } else { // else, parse current header - this stores the parsed value.
                    http_request_header *raw_header = (http_request_header *)calloc(1, sizeof(http_request_header));
                    // http request requirements.
                    bool host_found = false;
                    if(!_validate_http_header(header_line, num_parsed, &host_found, &raw_header)) { 
                        ERROR_LOG("Invalid header value was encountered, ignoring header.");
                        if(raw_header != NULL)
                            free(raw_header);
                    }

                    if(host_found && raw_header->valid)
                        (*result_metadata)->http_host = strndup(raw_header->value, raw_header->value_size);

                    validated_headers[validated_cursor] = raw_header;
                    ++validated_cursor; 
                }

                // end if followed by final double new carriage to end header section.
                if(*(raw_cursor + 2) == '\r'
                && *(raw_cursor + 3) == '\n')
                    break;

                // clear values so next iteration is clean.
                memset(header_buffer, 0, MAX_HTTP_HEADER_LINE_SIZE);
                num_parsed = 0;
                ++line_increment;
            } else {
                ERROR_LOG("Failure, could not parse headers - malformed value was found after a newline character.");
                return false;
            }

            raw_cursor += 2; // skip '\n'.
            continue;
        }

    }

    if(!(*result_metadata)->http_host) {
        ERROR_LOG("Failure, host header was not found. Denying request.");
        return false;
    }

    // add validated headers to the result.
    (*result_metadata)->num_headers = validated_cursor;
    (*result_metadata)->http_headers_size = (validated_cursor * sizeof(void *));
    (*result_metadata)->http_headers = malloc((*result_metadata)->http_headers_size); 
    memcpy((*result_metadata)->http_headers, validated_headers, (*result_metadata)->http_headers_size);

    return true;
}

static bool _route_http_request(char *message, size_t message_size, http_request **result_metadata) {
    if(!message
    || !result_metadata
    || !(*result_metadata)
    || !message_size) {
        ERROR_LOG("_get_http_metadata: Parameter provided was invalid.");
        return false;
    }

    DEBUG_LOG("_route_http_request: Reached end of current implementation.");

    // TODO: complete this once dynamic routing has been implemented.
    return true;
}

// clean and free data on failure.
bool _clean_header(http_request_header **header_instance) {
    if(!header_instance
    || !*header_instance) {
        ERROR_LOG("Failed to properly clean header after failure. Memory may have leaked.");
        return false;
    }

    http_request_header *header = *header_instance;
    header->key_size = 0;
    header->value_size = 0;
    header->valid = 0;
    if(header->key)
        free(header->key);
    if(header->value)
        free(header->value);

    return (
        !header->key_size
        && !header->value_size
        && !header->valid
        && !header->key
        && !header->value
    );
}


