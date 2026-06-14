#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#define MAX_HTTP_HEADER_SIZE 8192

typedef enum {
    GET,
    POST,
    PUT,
    DELETE 
} http_request_type;

typedef struct {
    http_request_type http_type;
    char **headers;
    char *http_body;
} http_request;

bool process_http_request(int socket_descriptor, char *message, size_t message_size, char **response);

#endif

