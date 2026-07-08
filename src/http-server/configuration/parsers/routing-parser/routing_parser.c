#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "services/logging/logging.h"
#include "utilities/error-handler/error_handler.h"

#include "./routing_parser.h"

static bool parse_routing_configurations(char *file_path) {
    if(file_path == NULL) {
        ERROR_LOG("parse_routing_configuration: Provided routing configuration file path was NULL.");
        return false;
    }


    
}

