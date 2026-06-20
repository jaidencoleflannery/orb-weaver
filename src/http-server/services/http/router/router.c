#include <stdbool.h>
#include <stdlib.h>

#include "services/logging/logging.h"
#include "configuration/configuration-handler/configuration_handler.h"

#include "./router.h"

// TODO: get max num of routes from configuration_handler.

static bool initialized = false;

// never free.
static route_metadata *configured_routes;

bool invoke_route(http_request request) {
    return true;
}

bool bind_routes() {


    return true;
}

bool initialize() {
    size_t max_num_routes = config.max_num_routes;
    if(max_num_routes < 1) {
        ERROR_LOG("initialize: Maximum number of routes must be a positive integer.");
        return false;
    }

    configured_routes = (route_metadata *)calloc(1, config.max_num_routes);
    if(configured_routes == NULL) {
        ERROR_LOG("initialize: Failed to allocate memory for configured_routes.");
        return false;
    }

    if(!bind_routes()) {
        ERROR_LOG("initialize: Failed to bind routes.");
        return false;
    }

    initialized = true;
    return true;
}

