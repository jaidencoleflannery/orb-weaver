#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "services/logging/logging.h"

#include "./hash_tools.h"

/*
 * TODO:
 * watch for collisions, if one is encountered, just iterate until an empty slot is found.
*/

static bool _hash_seek_slot(char *key, size_t key_size, hash_entry **table, size_t table_size, size_t hash_index, size_t result_index);
static bool _hash_seek_entry(char *key, size_t key_size, hash_entry **table, size_t table_size, size_t hash_index, size_t result_index);
static bool _generate_hash_index(char *key, size_t key_size, size_t table_size, size_t *hash_index);

// add entry. 
bool hash_add_entry(
    char *key, 
    size_t key_size, 
    void *value, 
    size_t value_size, 
    hash_entry **table, 
    size_t table_size, 
    size_t *hash_index // out parameter.
) {
    if(key == NULL) {
        ERROR_LOG("hash_add_entry: Provided key pointer was NULL.");
        return false;
    }

    if(hash_index == NULL) {
        ERROR_LOG("hash_add_entry: Provided hash_index pointer was NULL.");
        return false;
    }

    // generate hash index.
    *hash_index = 0;
    if(!_generate_hash_index(key, key_size, table_size, hash_index)) {
        ERROR_LOG("hash_add_entry: Failed to generate hash key for provided entry values.");
        *hash_index = 0;
        return false;
    } else if(*hash_index >= table_size) {
        ERROR_LOG("hash_add_entry: Unexpected error, generated hash key was too large.");
        return false;
    }

    table[*hash_index]->key = calloc(1, key_size);
    if(table[*hash_index]->key == NULL) {
        ERROR_LOG("hash_add_entry: Failed to allocate memory for key field.");
        return false;
    }

    table[*hash_index]->value = calloc(1, value_size); 
    if(table[*hash_index]->key == NULL) {
        ERROR_LOG("hash_add_entry: Failed to allocate memory for value field.");
        return false;
    }

    memcpy(table[*hash_index]->value, value, value_size);
    memcpy(table[*hash_index]->key, key, key_size);

    return true;
}

bool hash_remove_entry(
    char *key, 
    size_t key_size, 
    hash_entry **table, 
    size_t table_size
) {
    if(key == NULL) {
        ERROR_LOG("hash_remove_entry: Provided key pointer was NULL.");
        return false;
    }

    if(key_size < 1) {
        ERROR_LOG("hash_remove_entry: Provided key_size value was invalid.");
        return false;
    }

    size_t hash_index = 0;
    if(!_generate_hash_index(key, key_size, table_size, &hash_index)) {
        ERROR_LOG("hash_remove_entry: Failed to generate a hash index from provided values.");
        return false;
    } else if(hash_index < 0 || hash_index >= table_size) {
        ERROR_LOG("hash_remove_entry: Unexpected error, an invalid hash index was generated.");
        return false;
    }

    // if not found, see if elsewhere.
    if((*table)[hash_index].key == NULL) {
        size_t result_index = (table_size + 1);
        if(!_hash_seek_entry(key, key_size, table, table_size, hash_index, result_index)
        || result_index > table_size) {
            ERROR_LOG("hash_remove_entry: Failure, unable to locate key in hash table.");
            return false;
        }

        hash_index = result_index;
    }

    if((*table)[hash_index].key != NULL)
        free((*table)[hash_index].key);
    if((*table)[hash_index].value != NULL)
        free((*table)[hash_index].value);

    memset((*table + hash_index), 0, sizeof(hash_entry)); 
    return true;
}

bool hash_free(hash_entry **table, size_t table_size) {
    if(table == NULL) {
        ERROR_LOG("hash_free: Provided table was NULL.");
        return false;
    }

    hash_entry *table_cursor = *table;
    size_t table_counter = 0;
    while(table_cursor != NULL && table_counter < table_size) {
        if(table_cursor->key != NULL)
            free(table_cursor->key);

        if(table_cursor->value != NULL)
            free(table_cursor->value);

        ++table_cursor;
        ++table_counter;
    }

    free(*table);
    *table = NULL;

    return true;
}

bool hash_allocate(size_t num_entries, hash_entry **table) {
    if(table == NULL) {
        ERROR_LOG("allocate_hash: Provided hash entry was NULL.");
        return false;
    }

    *table = calloc(num_entries, sizeof(hash_entry));
    if(*table == NULL) {
        ERROR_LOG("allocate_hash: Failed to allocate memory for hash-table.");
        return false;
    }

    return true;
}

// find the first empty slot.
static bool _hash_seek_slot(
    char *key,
    size_t key_size,
    hash_entry **table,
    size_t table_size,
    size_t hash_index,
    size_t result_index
) {

    return true;
}

// find a specific key.
static bool _hash_seek_entry(
    char *key, 
    size_t key_size, 
    hash_entry **table, 
    size_t table_size, 
    size_t hash_index, 
    size_t result_index
) {

    return true;
}

static bool _generate_hash_index(
    char *key, 
    size_t key_size, 
    size_t table_size, 
    size_t *hash_index
) {
    if(key == NULL) {
        ERROR_LOG("_generate_hash_index: Provided key pointer was NULL.");
        return false;
    }

    if(key_size < 1) {
        ERROR_LOG("_generate_hash_index: Provided key size was invalid, must be a positive integer less than (%d).", MAX_KEY_LENGTH);
        return false;
    }

    if(hash_index == NULL) {
        ERROR_LOG("_generate_hash_index: Provided hash index pointer was NULL.");
        return false;
    }
 
    // iterate on key index,
    // each iteration scales by salt (prime num) and adds the current character value.
    long keygen_value = 0;
    char *key_cursor = key; 
    while(key_cursor != NULL && *key_cursor != '\0')
        keygen_value = keygen_value * KEYGEN_SALT + (long)*key_cursor;

    if(keygen_value == 0) {
        ERROR_LOG("_generate_hash_index: Unexpected error, generated key hash was not mutated properly.");
        return false;
    }

    // absolute value.
    int keygen_remainder = (keygen_value % table_size);
    *hash_index = (keygen_remainder > 0)
        ? keygen_remainder
        : -(keygen_remainder);

    return true;
}
