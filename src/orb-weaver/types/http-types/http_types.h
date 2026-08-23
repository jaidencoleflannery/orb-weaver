#ifndef HTTP_TYPES_H
#define HTTP_TYPES_H

#define MAX_HTTP_HEADER_LINE_SIZE 8192 // arbitrary.
#define MAX_HTTP_HEADER_COUNT 100 // arbitrary.
#define MAX_HTTP_HEADER_TOTAL_SIZE MAX_HTTP_HEADER_LINE_SIZE * MAX_HTTP_HEADER_COUNT

#define MAX_HTTP_BODY_SIZE 1.049e6f // arbitrary.

typedef enum {
    VERSION_0_9, // default
    VERSION_1_0,
    VERSION_1_1,
    VERSION_COUNT, // num protocols.
    VERSION_NULL
} http_request_protocol;

typedef enum {
    TYPE_NULL,
    TYPE_GET,
    TYPE_POST,
    TYPE_PUT,
    TYPE_DELETE, 
    TYPE_COUNT // num types. 
} http_request_method;

typedef struct { 
    size_t key_size;
    size_t value_size;
    char   *key; 
    char   *value;
    bool   valid;
} http_request_header;

typedef struct {
    http_request_method http_method;
    http_request_protocol http_protocol;
    http_request_header *http_headers; 
    size_t http_route_size;
    size_t http_headers_size;
    size_t http_body_size;
    char   *http_route;
    char   *http_body;
} http_request;

bool allocate_http_request(http_request **http_request_instance);

bool free_http_request(http_request *http_request_instance);

#endif

