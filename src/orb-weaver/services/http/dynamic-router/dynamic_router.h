#ifndef DYNAMIC_ROUTER_H
#define DYNAMIC_ROUTER_H

#include "types/http-types/http_types.h"

#define ROUTE_FOLDER "routes"
#define READ_ONLY "r"

typedef struct {
    http_request_method method;
    size_t path_size;
    char path[]; // always store inline.
} route_metadata;

typedef struct {   
    http_request request;
    route_metadata path;
} http_routing_payload;

bool initialize();

bool seek_routes(
    http_request_method method, 
    http_request request
);

#endif

