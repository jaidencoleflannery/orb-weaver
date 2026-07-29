#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#define PROTOCOL "HTTP"

#include "types/http-types/http_types.h"

// method lookup table item.
typedef struct {
    char                *name; 
    http_request_method method;
} method_entry;

// protocol lookup table item.
typedef struct {
    char                *name;
    http_request_protocol protocol;
} protocol_entry;

// for header parsing.
// changes to this enum can break validation logic.
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

// TODO: implemented HTTP/2 and HTTP/3 (different sending schema, needs own parser).
static const protocol_entry protocol_entries[] = {
    { "",           VERSION_0_9 }, // default.
    { "HTTP/1.0",   VERSION_1_0 },
    { "HTTP/1.1",   VERSION_1_1 },
};

static const 

bool process_http_request(int socket_descriptor, char *message, size_t message_size, char **response);

#endif

