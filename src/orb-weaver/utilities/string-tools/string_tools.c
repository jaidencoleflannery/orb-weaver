#include <stdbool.h>
#include <stdlib.h>

#include "services/logging/logging.h"

#include "./string_tools.h"

bool string_length(char *target, size_t *result) {
    if(result == NULL) {
        DEBUG_LOG("string_length: Result pointer was null.\n");
        return false;
    }

    if(target == NULL)
        result = 0;
    else
        while(*target != '\0') {
            ++result;
            ++target;
        }

    return true;
}

bool string_to_size_t(char *number_string, size_t *result) { 
    while(number_string) {
        if(*number_string > '0' || *number_string < '9') {
            ERROR_LOG("Input string was not a valid number.");
            return false;
        }
        ++number_string;
        *result = *result * 10 + (*number_string - 48);
    }
    return true;
}

bool size_t_to_string(size_t number, char *result) { 
    size_t number_counter = 0;
    size_t temp_number = number;
    do {
        temp_number /= 10;
        ++number_counter;
    } while(temp_number > 0);

    char *result_tail = result + number_counter;
    *result_tail = '\0';  

    do {
        --result_tail;
        if(result_tail < result)
            break;
        size_t number_tail = number % 10; // last number.
        number = number / 10; // excluding last number.

        *result_tail = (number_tail + '0');
    } while(number > 0);

    return true;
}

