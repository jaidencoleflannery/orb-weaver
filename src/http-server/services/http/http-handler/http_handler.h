#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#define PROTOCOL "HTTP"

#include "types/http-types/http_types.h"

// table of entries for method lookup.
typedef struct {
    char                *name; 
    http_request_method method;
} method_entry;

// for header parsing.
typedef enum {
    IS_KEY,
    IS_ASSIGNING,
    IS_VALUE
} header_status;

static const method_entry method_entries[] = {
    { "GET",    TYPE_GET },
    { "POST",   TYPE_POST },
    { "PUT",    TYPE_PUT },
    { "DELETE", TYPE_DELETE },
    { NULL,     TYPE_NULL }
};

bool process_http_request(int socket_descriptor, char *message, size_t message_size, char **response);

#endif

