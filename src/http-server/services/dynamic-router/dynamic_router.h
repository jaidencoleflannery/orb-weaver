#ifndef DYNAMIC_ROUTER_H
#define DYNAMIC_ROUTER_H

#include "types/http-types/http_types.h"

typedef struct {
    http_request_method method;
    size_t path_size;
    char path[]; 
} route;

typedef struct {   
    http_request request;
    route path;
} http_routing_payload;

bool initialize();

bool seek_routes(http_request_method method, http_request request);

#endif

