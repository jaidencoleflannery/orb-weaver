#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "services/logging/logging.h"
#include "configuration/configuration-handler/configuration_handler.h"

#include "./dynamic_router.h"

static bool initialized = false;

// never free.
static route_metadata **configured_routes;

static size_t num_routes = 0;
static size_t max_num_routes = 0;

static bool _allocate_routes() {
    max_num_routes = config.max_num_routes;
    if(max_num_routes < 1) {
        ERROR_LOG("initialize: Maximum number of routes must be a positive integer.");
        return false;
    }

    *configured_routes = (route_metadata *)calloc(1, (config.max_num_routes * sizeof(char *)));
    if(configured_routes == NULL) {
        ERROR_LOG("initialize: Failed to allocate memory for configured_routes.");
        return false;
    }

    return true;
}

// initialize all found routes.
static bool _initialize_routes() {
    return true;
}

// add a new route to table with hash for lookup.
bool bind_route(
    http_request_method method,
    size_t path_size,
    char *path
) {
    if(!initialized) {
        ERROR_LOG("bind_route: Router has not been initialized.");
        return false;
    }

    if(configured_routes == NULL) {
        ERROR_LOG("bind_route: Memory has not been properly allocated for routes.");
        return false;
    }

    if(method >= TYPE_COUNT
    || path_size < 1
    || path == NULL) {
        ERROR_LOG("bind_route: Invalid route data was provided.");
        return false;
    }

    // allocate and copy.

    configured_routes[num_routes] = (route_metadata *)calloc(1, sizeof(route_metadata) + path_size);
    if(configured_routes[num_routes] == NULL) {
        ERROR_LOG("bind_route: Failed to allocate memory for configured_routes at index [%zu].\n", num_routes);
        return false;
    }

    configured_routes[num_routes] = &(route_metadata) {
        .method = method,
        .path_size = path_size 
    };
    memcpy(configured_routes[num_routes]->path, path, path_size);
    if(configured_routes[num_routes] == NULL) {
        ERROR_LOG("bind_route: Failed to copy path into configured route at index [%zu].\n", num_routes);
        return false;
    } 

    ++num_routes;
    return true;
}

// invoke route function.
// if num_routes < 10 linear search, else hash lookup.
bool invoke_route(http_request request) {
    if(!initialized) {
        ERROR_LOG("invoke_route: Router has not been initialized.");
        return false;
    }

    return true;
}

bool initialize() {
    if(!_allocate_routes()) {
        ERROR_LOG("initialize: Failed to bind routes.");
        return false;
    }

    if(!)

    initialized = true;
    return true;
}

