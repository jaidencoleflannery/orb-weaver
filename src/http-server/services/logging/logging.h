#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

#define LOG(tag, input, ...) printf("@   " tag " " input "\n", ##__VA_ARGS__)

#define ERROR_LOG(input, ...) fprintf(stderr, "!   [ ERROR ] " input "\n", ##__VA_ARGS__)

#ifndef NDEBUG
#define DEBUG_LOG(input, ...) fprintf(stderr, ">   [ DEBUG ] " input "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(input) ((void)0)
#endif

#endif

