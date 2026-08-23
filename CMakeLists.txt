set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_minimum_required(VERSION 3.20)
project(
    orb 
    VERSION 1.0
    DESCRIPTION "A multithreaded HTTP reverse proxy."
    LANGUAGES C
)

add_library(
    server
    ./src/orb-weaver/services/orchestration/orchestrator/orchestrator.c
    ./src/orb-weaver/services/orchestration/thread-handler/thread_handler.c
    ./src/orb-weaver/services/http/http-handler/http_handler.c
    ./src/orb-weaver/services/tcp/host-resolver/host_resolver.c
    ./src/orb-weaver/services/tcp/connection-handler/connection_handler.c 
    ./src/orb-weaver/types/http-types/http_types.c
    ./src/orb-weaver/configuration/parsers/configuration-handler/configuration_handler.c
    ./src/orb-weaver/utilities/string-tools/string_tools.c 
    ./src/orb-weaver/utilities/error-handler/error_handler.c
)

add_executable(
    orb
    ./src/orb-weaver/main.c
)

set(CMAKE_BUILD_TYPE Debug)

target_compile_options(orb PRIVATE -Wall -Wextra -Wpedantic -std=c17)

target_include_directories(orb PRIVATE "./src/orb-weaver/")
target_include_directories(server PRIVATE "./src/orb-weaver/")
target_link_libraries(orb PRIVATE server)

