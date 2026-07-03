#ifndef CONFIGURATION_HANDLER_H
#define CONFIGURATION_HANDLER_H

#include <stddef.h>

#ifndef NDEBUG
#define CONFIG_FILE "./configuration/release.cfg"
#else
#define CONFIG_FILE "./configuration/development.cfg"
#endif

#define CONFIG_FOLDER "configuration"
#define READ_ONLY "r"

#define DEFAULT_MAX_CONNECTIONS 4096
#define DEFAULT_PORT 8888
#define DEFAULT_NUM_CORES 8
#define DEFAULT_MAX_NUM_ROUTES 256

#define MAX_VALUE_LENGTH 20
#define MAX_FIELD_LENGTH 200

typedef enum {
    CONFIG_MAX_CONNECTIONS,
    CONFIG_PORT,
    CONFIG_NUM_CORES,
    CONFIG_MAX_NUM_ROUTES,
    CONFIG_NULL
} config_values;

// keep fields the same data type so validation is straightforward.
typedef struct {
    size_t  max_connections;
    size_t  port;
    size_t  num_cores;
    size_t  max_num_routes;
} configuration;

// table of entries for lookup.
typedef struct {
    char    *name;
    size_t  offset; // offset into struct configuration where the field exists.
    config_values type;
} cfg_entry;

static const cfg_entry cfg_entries[] = {
    { "max_connections",    offsetof(configuration, max_connections),   CONFIG_MAX_CONNECTIONS },
    { "port",               offsetof(configuration, port),              CONFIG_PORT },
    { "num_cores",          offsetof(configuration, num_cores),         CONFIG_NUM_CORES },
    { "max_num_routes",     offsetof(configuration, max_num_routes),    CONFIG_MAX_NUM_ROUTES },
    { NULL,                 0,                                          CONFIG_NULL },
};

extern configuration config;

bool initialize_configuration(void);

bool fetch_configuration_by_name(char *target, size_t *value);

bool fetch_configuration_by_enum(config_values target, size_t *value);

#endif

