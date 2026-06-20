#include <stdlib.h>
#include <stdbool.h>

#include "services/logging/logging.h"

#include "./hash_tools.h"

static bool _generate_key(char *key, size_t key_size, size_t table_size, size_t *hash_index) {
    if(key == NULL) {
        ERROR_LOG("_generate_key: Provided key pointer was NULL.");
        return false;
    }

    if(key_size < 1) {
        // clangd does not like this line; will still compile.
        ERROR_LOG("_generate_key: Provided key size was invalid, must be a positive integer less than (%d).", MAX_KEY_LENGTH);
        return false;
    }

    if(hash_index == NULL) {
        ERROR_LOG("_generate_key: Provided hash index pointer was NULL.");
        return false;
    }

    long keygen_value = 0;

    char *key_cursor = key;
    while(key_cursor != NULL && *key_cursor != '\0')
        keygen_value += (long)*key_cursor;

    

    return true;
}

bool hash_allocate(size_t num_entries, hash_entry **result) {
    if(result == NULL) {
        ERROR_LOG("allocate_hash: Provided hash entry was NULL.");
        return false;
    }

    *result = calloc(1, (num_entries * sizeof(hash_entry)));
    if(*result == NULL) {
        ERROR_LOG("allocate_hash: Failed to allocate memory for hash-table.");
        return false;
    }

    return true;
}

bool hash_add_entry(char *key, size_t key_size, hash_entry *table, size_t table_size, size_t *hash_index) {
    if(key == NULL) {
        ERROR_LOG("hash_add_entry: Provided key pointer was NULL.");
        return false;
    }

    if(hash_index == NULL) {
        ERROR_LOG("hash_add_entry: Provided hash_index pointer was NULL.");
        return false;
    }

    size_t hash_key_store = 0;

    if(!_generate_key(key, key_size, table_size, &hash_key_store)) {
        ERROR_LOG("hash_generate_key: Failed to generate hash key for provided entry values.");
    }

    return true;
}

// TODO.
bool hash_free(hash_entry *key) { 

    return true;
}

