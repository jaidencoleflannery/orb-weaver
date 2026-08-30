#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "services/logging/logging.h"

#include "./hash_tools.h"

static bool _hash_seek_entry(char *key, size_t key_size, hash_entry **table, size_t table_size, size_t hash_index, size_t *result_index);
static bool _generate_hash_index(char *key, size_t key_size, size_t table_size, size_t *hash_index);

/*
 * basic open address hash map implementation.
 * never allow the 0th index to be filled (hash index error state).
*/

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
    if(!key
    || !value 
    || key_size < 1
    || value_size < 1) {
        ERROR_LOG("hash_add_entry: Provided entry values were invalid.");
        return false;
    }

    if(!table
    || table_size < 1) {
        ERROR_LOG("hash_add_entry: Failure, provided table was in an invalid state.");
        return false;
    }

    if(!hash_index) {
        ERROR_LOG("hash_add_entry: Provided pointer for the result hash index was invalid.");
        return false;
    }

    table_size *= 2; // match actual allocation.

    // generate hash index.
    *hash_index = 0;
    if(!_generate_hash_index(key, key_size, table_size, hash_index)) {
        ERROR_LOG("hash_add_entry: Failed to generate hash key for provided entry values.");
        *hash_index = 0;
        return false;
    }
    if(*hash_index >= table_size || *hash_index == 0) {
        ERROR_LOG("hash_add_entry: Unexpected error, generated hash key was invalid.");
        *hash_index = 0;
        return false;
    }

    // collision logic (if an index contains a pointer, it is a route).
    if(table[*hash_index]) {
        DEBUG_LOG("hash_add_entry: Collision, generated hash was not unique. Seeking empty slot.");
        size_t probe_counter = *hash_index;
        size_t step_counter = 0;
        while(table[probe_counter]) { 
            if(++step_counter >= table_size) { // full loop.
                ERROR_LOG("hash_add_entry: Critical failure, no empty hash slots were found.");
                return false;
            }

            if(++probe_counter >= table_size) { 
                probe_counter = 1; // skip 0.
                ++step_counter; // count 0 as a step.
            }
        }

        *hash_index = probe_counter;
    }

    // flatten struct.

    table[*hash_index] = (hash_entry *)calloc(1, (sizeof(hash_entry) + (key_size + 1) + (value_size + 1)));
    if(!table[*hash_index]) {
        ERROR_LOG("hash_add_entry: Failure, could not allocate memory for hash index.");
        return false;
    }

    hash_entry *entry = (table[*hash_index]);

    entry->key_size = key_size;
    entry->value_size = value_size;
     
    char *data_offset = ((char *)entry + sizeof(hash_entry));

    // set pointers to memory block offset.
    entry->key = data_offset;
    entry->value = (data_offset + (key_size + 1));
    memcpy(entry->key, key, key_size);
    entry->key[key_size] = '\0';
    memcpy(entry->value, value, value_size);
    entry->value[value_size] = '\0';

    return true;
}

bool hash_remove_entry(
    char *key, 
    size_t key_size, 
    hash_entry **table, 
    size_t table_size
) {
    if(!key
    || key_size < 1) {
        ERROR_LOG("hash_remove_entry: Provided key values were invalid.");
        return false;
    }

    if(!table
    || table_size < 1) {
        ERROR_LOG("hash_remove_entry: Failure, provided table was in an invalid state.");
        return false;
    }

    table_size *= 2; // match actual allocation.

    size_t hash_index = 0;
    if(!_generate_hash_index(key, key_size, table_size, &hash_index)) {
        ERROR_LOG("hash_remove_entry: Failed to generate a hash index from provided values.");
        return false;
    }
    if(hash_index == 0 || hash_index >= table_size) {
        ERROR_LOG("hash_remove_entry: Unexpected error, an invalid hash index was generated.");
        return false;
    } 

    // check if key value is in hash index, if not, linear search.
    if(!table[hash_index] 
    || strcmp(table[hash_index]->key, key) != 0) {
        size_t result_index = table_size;
        if(!_hash_seek_entry(key, key_size, table, table_size, hash_index, &result_index)
        || result_index == 0
        || result_index >= table_size) {
            ERROR_LOG("hash_remove_entry: Failure, unable to locate key in hash table.");
            return false;
        }

        hash_index = result_index;
    }

    free(table[hash_index]);
    table[hash_index] = NULL;

    return true;
}

bool hash_fetch_entry(
    char *key,
    size_t key_size,
    hash_entry **table,
    size_t table_size,
    void **value // out parameter.
) {
    if(!key
    || key_size < 1) {
        ERROR_LOG("hash_fetch_entry: Failure, invalid values were provided.");
        return false;
    }

    if(!table
    || table_size < 1) {
        ERROR_LOG("hash_fetch_entry: Failure, provided table was in an invalid state.");
        return false;
    }

    if(!value) {
        ERROR_LOG("hash_fetch_entry: Failure, provided result pointer was NULL.");
        return false;
    }

    table_size *= 2; // match actual allocation.

    size_t hash_index = 0;
    if(!_generate_hash_index(key, key_size, table_size, &hash_index)) {
        ERROR_LOG("hash_fetch_entry: Failed to generate a hash index from provided values.");
        return false;
    }
    if(hash_index == 0 || hash_index >= table_size) {
        ERROR_LOG("hash_fetch_entry: Unexpected error, an invalid hash index was generated.");
        return false;
    }

    // check if key is in index, if not, linear search.
    if(!table[hash_index] || strcmp(table[hash_index]->key, key) != 0) {
        size_t result_index = 0;
        if(!_hash_seek_entry(key, key_size, table, table_size, hash_index, &result_index)
        || result_index >= table_size
        || result_index == 0) {
            ERROR_LOG("hash_fetch_entry: Failure, unable to locate key in hash table.");
            return false;
        }

        hash_index = result_index;
    }

    if(table[hash_index]) {
        // gives caller direct access (better than allocating hidden memory).
        *value = table[hash_index]->value;
        return true;
    } else {
        ERROR_LOG("hash_fetch_entry: Unexpected error, hash fetch logic fell through to an invalid state.");
        return false;
    }
}

bool hash_free(hash_entry ***table, size_t table_size) {
    if(!table
    || !*table
    || table_size < 1) {
        ERROR_LOG("hash_free: Provided table was in an invalid state.");
        return false;
    }

    table_size *= 2; // match actual allocation.

    for(size_t cursor = 0; cursor < table_size; cursor++)
        if(*table[cursor]) {
            free(*table[cursor]);
            *table[cursor] = NULL;
        }
 
    free(*table);
    *table = NULL; // have the caller handle this?

    return true;
}

bool hash_allocate(size_t num_entries, hash_entry ***table) {
    if(*table) {
        ERROR_LOG("hash_allocate: Error, provided table pointer was already allocated.");
        return false;
    }

    if(num_entries < 2) {
        ERROR_LOG("hash_allocate: Error, provided values were invalid.");
        return false;
    }

    // increase the number of slots so that collisions cannot destroy the map.
    *table = calloc((num_entries * 2), sizeof(hash_entry *));
    if(*table == NULL) {
        ERROR_LOG("allocate_hash: Failed to allocate memory for hash-table.");
        return false;
    }

    return true;
}

// find a specific key.
static bool _hash_seek_entry(
    char *key,
    size_t key_size,
    hash_entry **table,
    size_t table_size,
    size_t hash_index,
    size_t *result_index
) {
    *result_index = 0;

    size_t probe_counter = hash_index;
    for(size_t step_counter = 0; step_counter < table_size; ++step_counter) {
        if(++probe_counter >= table_size) { 
            probe_counter = 1; // skip 0.
            ++step_counter; // count 0 as a step.
        }

        if(table[probe_counter] 
        && strcmp(table[probe_counter]->key, key) == 0) {
            *result_index = probe_counter;
            return true;
        }
    }

    ERROR_LOG("hash_seek_entry: Failure, hash value was not found within table.");
    return false;
}

static bool _generate_hash_index(
    char *key, 
    size_t key_size, 
    size_t table_size, 
    size_t *hash_index
) {
    if(!key
    || key_size < 1
    || table_size < 2) {
        ERROR_LOG("_generate_hash_index: Provided values were invalid.");
        return false;
    }

    if(!hash_index) {
        ERROR_LOG("_generate_hash_index: Provided hash index pointer was NULL.");
        return false;
    }

    // iterate on key index,
    size_t keygen_value = 0; 
    for(size_t cursor = 0; cursor < key_size; cursor++) 
        keygen_value = ((keygen_value * KEYGEN_SALT) + (size_t)key[cursor]); // unsigned overflow wraps.
 
    size_t result = (keygen_value % table_size); // modulo scales to fit in table.
    *hash_index = (!result) // 0 is invalid.
        ? ++result
        : result;

    return true;
}

