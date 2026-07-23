#include <stdlib.h>
#include <stdbool.h>

#include "services/logging/logging.h"

#include "./http_types.h"

// param is a pointer to the memory.
bool allocate_http_request(http_request **http_request_instance) {
    if(http_request_instance == NULL) {
        ERROR_LOG("allocate_http_request: Provided http_request pointer was NULL.");
        return false;
    }

    *http_request_instance = (http_request *)calloc(1, sizeof(http_request));
    if(*http_request_instance == NULL) {
        ERROR_LOG("allocate_http_request: Failed to allocate memory for http_request instance.");
        return false;
    }

    (*http_request_instance)->http_method = TYPE_NULL;

    // caller is expected to allocate their own headers as needed.
    // the included free function in this unit covers any allocation.
    (*http_request_instance)->http_headers_size = 0;
    (*http_request_instance)->http_headers = calloc(MAX_HTTP_HEADER_COUNT, sizeof(http_request_header));
    if((*http_request_instance)->http_headers == NULL) {
        ERROR_LOG("allocate_http_request: Failed to allocate memory for http_headers.");
        return false;
    }

    (*http_request_instance)->http_route_size = 0;
    (*http_request_instance)->http_route = calloc(1, sizeof(char *) * MAX_HTTP_HEADER_SIZE);
    if((*http_request_instance)->http_route == NULL) {
        ERROR_LOG("allocate_http_request: Failed to allocate memory for http_route.");
        return false;
    }

    (*http_request_instance)->http_body_size = 0;
    (*http_request_instance)->http_body = NULL; // body has to be dynamically handled by user.
    
    return true;
}

bool free_http_request(http_request *http_request_instance) {
    if(http_request_instance == NULL) {
        ERROR_LOG("free_http_request: Provided http_request pointer was NULL.");
        return false;
    }

    bool function_result = true;
    
    if(http_request_instance->http_route != NULL)
        free(http_request_instance->http_route);
    if(http_request_instance->http_route != NULL) {
        ERROR_LOG("free_http_request: Failed to free http_route.");
        function_result = false;
    }

    // clear any allocation the caller performed.
    http_request_header *header_cursor = http_request_instance->http_headers;
    while(header_cursor != NULL) {
        free(header_cursor);
        if(header_cursor != NULL) {
            ERROR_LOG("free_http_request: Failed to free child header.");
            function_result = false;
        }
        ++header_cursor;
    }

    if(http_request_instance->http_headers != NULL)
        free(http_request_instance->http_headers);
    if(http_request_instance->http_headers != NULL) {
        ERROR_LOG("free_http_request: Failed to free http_headers.");
        function_result = false;
    }

    free(http_request_instance);
    if(http_request_instance != NULL) {
        ERROR_LOG("free_http_request: Failed to free http_headers.");
        function_result = false;
    }

    return function_result;
}

