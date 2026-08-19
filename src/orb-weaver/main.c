#include <stdlib.h>
#include <stdbool.h>

#include "services/logging/logging.h"
#include "services/orchestration/orchestrator/orchestrator.h"
#include "services/orchestration/thread-handler/thread_handler.h"

int main(void) {
    if(!boot_server()) {
        ERROR_LOG("main: Failed to boot.");
        free_thread_memory();
        return EXIT_FAILURE;
    }

    if(!start_processing()) {
        ERROR_LOG("main: Failed to start processing."); 
        free_thread_memory();
        return EXIT_FAILURE;
    }

    // cleanup heap allocated memory from thread pool.
    free_thread_memory();

    LOG("[ ORB ]", "Process exited.");
    return EXIT_SUCCESS;
}

