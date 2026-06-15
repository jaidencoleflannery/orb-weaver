#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#define PROTOCOL "HTTP"

#define IS_KEY 0
#define IS_ASSIGNING 1
#define IS_VALUE 2

#include "types/http-types/http_types.h"

// table of entries for method lookup.
typedef struct {
    char                *name; 
    http_request_method method;
} method_entry;

static const method_entry method_entries[] = {
    { "GET",    TYPE_GET },
    { "POST",   TYPE_POST },
    { "PUT",    TYPE_PUT },
    { "DELETE", TYPE_DELETE },
    { NULL,     TYPE_NULL }
};

bool process_http_request(int socket_descriptor, char *message, size_t message_size, char **response);

#endif

