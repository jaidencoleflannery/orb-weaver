#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "services/logging/logging.h"

#include "./error_handler.h"

bool validate_syscall(int value, char *caller, char *error_message) {
    if(caller == NULL) {
        ERROR_LOG("validate_syscall: The caller parameter provided was invalid. Provided error message: %s.\n", error_message);
        return false;
    }

    if(value < 0) {
        char *socket_error = strerror(errno);
        if(socket_error == NULL)
            ERROR_LOG("%s: %s Error: `errno` could not be properly parsed.\n", caller, error_message);
        else
            ERROR_LOG("%s: %s Error: %s\n", caller, error_message, socket_error);
        return false;
    }

    return true;
}

