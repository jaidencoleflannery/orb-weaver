#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/sysctl.h>

#include "services/logging/logging.h"
#include "utilities/error-handler/error_handler.h"

#include "./configuration_handler.h"

// configuration values, defaults on init.
configuration config = (configuration){
    .max_connections = DEFAULT_MAX_CONNECTIONS,
    .port = DEFAULT_PORT,
    .num_cores = DEFAULT_NUM_CORES,
    .max_num_routes = DEFAULT_MAX_NUM_ROUTES
};

// TODO: parse max_num_routes from config (not currently setup).

static bool validate_field_name(char *name, cfg_entry *entry_field) {
    const cfg_entry *valid_entries = cfg_entries;
    while(valid_entries->name != NULL) {
        if(strcmp(name, valid_entries->name) == 0) {
            *entry_field = *valid_entries;
            return true; 
        }
        ++valid_entries;
    }
    return false;
}

static bool find_enum_row(config_values target, cfg_entry *entry_field) {
    const cfg_entry *valid_entries = cfg_entries;
    while(valid_entries->type != CONFIG_NULL) {
        if(target == valid_entries->type) {
            *entry_field = *valid_entries;
            return true; 
        }
        ++valid_entries;
    }
    return false;
}

static bool parse_configuration(void) { 
    // validate folder path.
    struct stat status;
    if(stat(CONFIG_FOLDER, &status) != 0) {
        ERROR_LOG("Configuration folder could not be found. This program expects a `./configuration/` folder in root. Error: `%s`.", strerror(errno));
        return false;
    }
 
    FILE *configuration_file = fopen(CONFIG_FILE, READ_ONLY);
    if(configuration_file == NULL) {
        ERROR_LOG("Could not open configuration file from folder: `./configuration/`. Error: `%s`.", strerror(errno));
        return false;
    } 

    // read.
    char line[MAX_FIELD_LENGTH] = { 0 };
    while(fgets(line, MAX_FIELD_LENGTH, configuration_file)) {
        char *line_cursor = line;

        char field_name_cache[MAX_FIELD_LENGTH] = { 0 };
        // get field name. 
        for(int char_index = 0; char_index < MAX_FIELD_LENGTH; char_index++) {
            if(*line_cursor == ' ')
                break;

            field_name_cache[char_index] = *line_cursor;
            ++line_cursor;
        }

        DEBUG_LOG("initialize_configuration: Read configuration field name: `%s`.", field_name_cache);

        cfg_entry entry_field;

        if(!validate_field_name(field_name_cache, &entry_field)) {
            ERROR_LOG("Could not validate name of configuration field: `%s`.", field_name_cache);
            return false;
        }

        // manually push through assignment section.
        ++line_cursor;
        if(*line_cursor != '=') {
            ERROR_LOG("Configuration file is malformed around field name: `%s`.", field_name_cache);
            return false;
        }
        ++line_cursor;
        if(*line_cursor != ' ') {
            ERROR_LOG("Configuration file is malformed around field name: `%s`.", field_name_cache);
            return false;
        }
        ++line_cursor;
        if(*line_cursor < '0' || *line_cursor > '9') {
            ERROR_LOG("Configuration file is malformed around field name: `%s`.", field_name_cache);
            return false;
        }

        // get field value.
        char field_value_cache[MAX_VALUE_LENGTH] = { 0 }; // the null term and newline are left off of value.
        for(int char_index = 0; char_index < (MAX_VALUE_LENGTH); char_index++) {
            if(!*line_cursor || *line_cursor == '\n')
                break;

            field_value_cache[char_index] = *line_cursor;
            ++line_cursor;
        }

        DEBUG_LOG("initialize_configuration: Read configuration field value: `%s`.", field_value_cache);

        // convert string value into a number.
        char *value_cursor = field_value_cache;
        size_t value = 0;
        while(value_cursor && *value_cursor) {
            int current_value = (int)*value_cursor;
            if(current_value < 48 || current_value > 57) {
                ERROR_LOG("Configuration field value is invalid, values must all be positive non-zero integer types.");
                return false;
            }

            value = value * 10 + (current_value - '0');
            ++value_cursor;
        }

        if(value <= 0) {
            ERROR_LOG("Configuration field value is invalid, values must all be positive non-zero integer types.");
            return false;
        }

        memcpy((unsigned char *)&config + entry_field.offset, &value, sizeof(value));
        LOG("[ Configuration ]", "Set value `%s` = %zu.", field_name_cache, value);
        DEBUG_LOG("Value directly from cache: %zu, %zu.", config.max_connections, config.port);
    } 
    
    DEBUG_LOG("initialize_configuration: Read configuration.\nmax_connections: %zu.\n port: %zu.\nnum_cores: %zu", config.max_connections, config.port, config.num_cores);
    return true;
}

bool initialize_configuration(void) {
    if(!parse_configuration()) {
        ERROR_LOG("Error parsing configuration, defaulting to default configuration.");
        return false;
    }

    DEBUG_LOG("initialize_configuration: Configuration loaded successfully.");


    int32_t *num_cores = &(int32_t){ -1 };
    if(!validate_syscall(
        sysctlbyname("hw.perflevel0.physicalcpu", num_cores, &(size_t){ sizeof(*num_cores) }, 0, 0), 
        "initialize_configuration", 
        "Failed to fetch number of performance cores on current machine.") 
    )
        return false;

    if(*num_cores < 0) {
        ERROR_LOG("Failed to fetch number of performance cores on current machine.");
        return false;
    }

    config.num_cores = (size_t)*num_cores - 1; // leave one off so the system isn't fully exhausted.
    DEBUG_LOG("initialize_configuration: Fetched number of cores successfully (%zu).", config.num_cores);

    return true;
}

bool fetch_configuration_by_name(char *target, size_t *value) {
    cfg_entry entry_field;
    if(!validate_field_name(target, &entry_field)) {
        ERROR_LOG("Field name was invalid.");
        return false;
    }
    *value = *(size_t *)((unsigned char *)&config + entry_field.offset);
    return true;
}

bool fetch_configuration_by_enum(config_values target, size_t *value) {
    cfg_entry entry_field;
    if(!find_enum_row(target, &entry_field)) {
        ERROR_LOG("Field name was invalid.");
        return false;
    }
    *value = *(size_t *)((unsigned char *)&config + entry_field.offset);
    return true;
}

