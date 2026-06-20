#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "services/logging/logging.h"

#include "./hash_tools.h"

static bool _generate_key(char *key, size_t key_size, size_t table_size, size_t *hash_index) {
    if(key == NULL) {
        ERROR_LOG("_generate_key: Provided key pointer was NULL.");
        return false;
    }

    if(key_size < 1) {
        ERROR_LOG("_generate_key: Provided key size was invalid, must be a positive integer less than (%d).", MAX_KEY_LENGTH);
        return false;
    }

    if(hash_index == NULL) {
        ERROR_LOG("_generate_key: Provided hash index pointer was NULL.");
        return false;
    }

    long keygen_value = 0;

    char *key_cursor = key;
    // iterate on key, each index scales by salt (prime num) and adds the current character value.
    while(key_cursor != NULL && *key_cursor != '\0')
        keygen_value = keygen_value * KEYGEN_SALT + (long)*key_cursor;

    if(keygen_value == 0) {
        ERROR_LOG("_generate_key: Unexpected error, generated key hash was not mutated properly.");
        return false;
    }

    int keygen_remainder = (keygen_value % table_size);
    *hash_index = (keygen_remainder > 0)
        ? keygen_remainder
        : -(keygen_remainder);

    return true;
}

bool hash_allocate(size_t num_entries, hash_entry **table) {
    if(table == NULL) {
        ERROR_LOG("allocate_hash: Provided hash entry was NULL.");
        return false;
    }

    *table = calloc(1, (num_entries * sizeof(hash_entry)));
    if(*table == NULL) {
        ERROR_LOG("allocate_hash: Failed to allocate memory for hash-table.");
        return false;
    }

    return true;
}

// generate hash, store value and return hash index.
bool hash_add_entry(char *key, size_t key_size, void *value, hash_entry **table, size_t table_size, size_t *hash_index) {
    if(key == NULL) {
        ERROR_LOG("hash_add_entry: Provided key pointer was NULL.");
        return false;
    }

    if(hash_index == NULL) {
        ERROR_LOG("hash_add_entry: Provided hash_index pointer was NULL.");
        return false;
    }

    size_t hash_key = 0;
    if(!_generate_key(key, key_size, table_size, &hash_key)) {
        ERROR_LOG("hash_generate_key: Failed to generate hash key for provided entry values.");
    }

    table[hash_key] = calloc(1, (sizeof(hash_entry) + key_size));
    if(table[hash_key] == NULL) {
        ERROR_LOG("hash_add_entry: Failed to allocate memory for table index: (%zu).\n", hash_key);
        return false;
    }

    // hash might be losing value if it's stack allocated.
    table[hash_key]->value = value;
    memcpy(table[hash_key]->key, key, key_size);

    if(table[hash_key]->value == NULL
    || table[hash_key]->key == NULL) {
        ERROR_LOG("hash_add_entry: Failed to copy key-value pair into hash entry.");
        free(table[hash_key]);
        return false;
    }

    return true;
}

// TODO.
bool hash_free(hash_entry *key) { 

    return true;
}

