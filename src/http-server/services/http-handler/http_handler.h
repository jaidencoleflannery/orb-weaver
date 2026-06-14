#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#define MAX_HTTP_HEADER_SIZE 8192
#define PROTOCOL "HTTP"
#define PROTOCOL_LOWER "http"

typedef enum {
    TYPE_GET,
    TYPE_POST,
    TYPE_PUT,
    TYPE_DELETE,
    TYPE_NULL,
    TYPE_COUNT // sentinel.
} http_request_type;

// table of entries for type lookup.
typedef struct {
    char                *name; 
    http_request_type   type;
} type_entry;

static const type_entry type_entries[] = {
    { "GET",    TYPE_GET },
    { "POST",   TYPE_POST },
    { "PUT",    TYPE_PUT },
    { "DELETE", TYPE_DELETE },
    { NULL,     TYPE_NULL }
};

typedef struct {
    http_request_type   http_type;
    char                **headers;
    char                *http_body;
} http_request;

bool process_http_request(int socket_descriptor, char *message, size_t message_size, char **response);

#endif

