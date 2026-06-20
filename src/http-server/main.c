#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stddef.h>

#include "services/logging/logging.h"
#include "services/orchestration/orchestrator/orchestrator.c"

int main(void) {
    if(!boot_server()) {
        ERROR_LOG("main: Failed to boot.");
        return EXIT_FAILURE;
    }

    if(!start_processing()) {
        ERROR_LOG("main: Failed to start processing.");
        return EXIT_FAILURE;
    }

    LOG("[ ORB ]", "Process exited.");
    return EXIT_SUCCESS;
}

