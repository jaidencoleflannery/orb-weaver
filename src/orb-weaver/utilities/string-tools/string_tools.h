#ifndef STRING_TOOLS_H
#define STRING_TOOLS_H

bool string_length(char *target, size_t *result);

bool string_to_size_t(char *number_string, size_t *result);

bool size_t_to_string(size_t number, char *result);

#endif
