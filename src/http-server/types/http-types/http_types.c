#include <stdlib.h>
#include <stdbool.h>

#include "services/logging/logging.h"

#include "./http_types.h"

// param is a pointer to the memory.
bool allocate_http_request(http_request **http_request_instance) {
    if(http_request_instance == NULL) {
        ERROR_LOG("allocate_http_request: Provided http_request pointer was NULL.");
        return false;
    } else if(*http_request_instance != NULL) {
        ERROR_LOG("allocate_http_request: Provided http_request instance has already been allocated.");
        return false;
    }

    *http_request_instance = (http_request *)calloc(1, sizeof(http_request));
    if(*http_request_instance == NULL) {
        ERROR_LOG("allocate_http_request: Failed to allocate memory for http_request instance.");
        return false;
    }

    (*http_request_instance)->http_method = TYPE_NULL;

    (*http_request_instance)->http_headers_size = 0;
    (*http_request_instance)->http_headers = calloc(MAX_HTTP_HEADER_COUNT, sizeof(void *));
    if((*http_request_instance)->http_headers == NULL) {
        ERROR_LOG("allocate_http_request: Failed to allocate memory for http_headers.");
        return false;
    }

    char **header_cursor = (*http_request_instance)->http_headers;
    while(header_cursor != NULL) {
        *header_cursor = calloc(1, MAX_HTTP_HEADER_SIZE);
        if((*http_request_instance)->http_headers == NULL) {
            ERROR_LOG("allocate_http_request: Failed to allocate memory for header children.");
            return false;
        }
        ++header_cursor;
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
        ERROR_LOG("allocate_http_request: Provided http_request pointer was NULL.");
        return false;
    }
    
    free(http_request_instance->http_route);


    return true;
}

