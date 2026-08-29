#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "services/logging/logging.h"
#include "utilities/hash-tools/hash_tools.h"
#include "configuration/parsers/configuration-handler/configuration_handler.h"

#include "./dynamic_router.h"

static bool initialized = false;

// never free.
static hash_entry *configured_routes;

static size_t num_routes = 0;
static size_t max_num_routes = 0;

static bool _get_key(char *path, size_t path_size, http_request_method method, char *key, size_t key_size);

bool initialize() {
    max_num_routes = config.max_num_routes;
    if(max_num_routes < 1) {
        ERROR_LOG("initialize: Maximum number of routes must be a positive integer greater than zero.");
        return (initialized = false);
    }

    hash_allocate(config.max_num_routes, &configured_routes);
    if(!configured_routes) {
        ERROR_LOG("initialize: Failed to allocate memory for configured_routes.");
        return (initialized = false);
    }

    return (initialized = true);
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

    if(!configured_routes) {
        ERROR_LOG("bind_route: Memory has not been properly allocated for routes.");
        return false;
    }

    if(method >= TYPE_COUNT 
    || !path
    || path_size < 1) {
        ERROR_LOG("bind_route: Invalid route data was provided.");
        return false;
    }

    if(num_routes > config.max_num_routes) {
        ERROR_LOG("bind_route: Maximum number of routes reached (consider augmenting configuration value).");
        return false;
    }

    // allocate route slot.
    configured_routes[num_routes].value = (route_metadata *)calloc(1, sizeof(route_metadata) + path_size);
    if(!(configured_routes[num_routes].value)) {
        ERROR_LOG("bind_route: Failed to allocate memory for configured_routes at index [%zu].\n", num_routes);
        return false;
    }

    size_t key_size = (path_size + 1);
    char key[key_size];

    // concat path + method.
    if(!_get_key(path, path_size, method, key, key_size)) {
        ERROR_LOG("bind_route: Failed to hash and store route, path key could not be properly generated.");
        return false;
    }

    // hash method persists memory, keep struct flat to avoid loss.
    size_t metadata_size = (sizeof(route_metadata) + path_size);
    route_metadata *metadata = (route_metadata *)calloc(1, metadata_size);
    if(!metadata) {
        ERROR_LOG("bind_route: Error, failed allocate memory for route value storage.");
        return false;
    }

    metadata->method = method;
    metadata->path_size = path_size;
    memcpy(metadata->path, path, path_size);

    size_t hash_index = -1;

    if(!hash_add_entry(
        key,
        key_size,
        &metadata,
        metadata_size,
        &configured_routes,
        config.max_num_routes,
        &hash_index
    )) {
        ERROR_LOG("bind_route: Failed to add route entry to hash table.");
        return false;
    }

    if()

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

static bool _get_key(
    char *path, 
    size_t path_size, 
    http_request_method method, 
    char *key, 
    size_t key_size
) {
    // key_size must be larger than path_size so value can be appended.
    if(key_size <= path_size) {
        ERROR_LOG("Failed to generate key, key's allocated memory was smaller than the provided path length.");
        return false;
    }

    char *source = path;
    http_request_method tag = method;
    while(*source)
        *key++ = *source++;
    
    // append method to path.
    *(++key) = (char)(tag + '0');

    return true;
}

