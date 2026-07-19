set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_minimum_required(VERSION 3.20)
project(
    orb 
    VERSION 1.0
    DESCRIPTION "An efficient HTTP server for MacOS, built in C."
    LANGUAGES C
)

add_library(
    server
    ./src/http-server/services/tcp/host-resolver/host_resolver.c
    ./src/http-server/services/tcp/connection-handler/connection_handler.c
    ./src/http-server/utilities/string-tools/string_tools.c 
    ./src/http-server/configuration/configuration-handler/configuration_handler.c
    ./src/http-server/services/orchestration/orchestrator/orchestrator.c
    ./src/http-server/utilities/error-handler/error_handler.c
    ./src/http-server/services/orchestration/thread-handler/thread_handler.c
)

add_executable(
    orb
    ./src/http-server/main.c
)

set(CMAKE_BUILD_TYPE Debug)

target_compile_options(orb PRIVATE -Wall -Wextra -Wpedantic -std=c17)

target_include_directories(orb PRIVATE "./src/http-server/")
target_include_directories(server PRIVATE "./src/http-server/")
target_link_libraries(orb PRIVATE server)

