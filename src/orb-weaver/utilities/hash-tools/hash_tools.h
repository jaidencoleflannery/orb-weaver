#ifndef HASH_TOOLS_H
#define HASH_TOOLS_H

#define MAX_KEY_LENGTH 256
#define KEYGEN_SALT 31

typedef struct {
    size_t key_size;
    size_t value_size;
    char   *key;
    char   *value;
} hash_entry;

bool hash_add_entry(
    char       *key, 
    size_t     key_size, 
    void       *value, 
    size_t     value_size, 
    hash_entry **table, 
    size_t     table_size, 
    size_t     *hash_index // out parameter.
);

bool hash_remove_entry(
    char       *key, 
    size_t     key_size, 
    hash_entry **table, 
    size_t     table_size
);

bool hash_fetch_entry(
    char       *key, 
    size_t     key_size, 
    hash_entry **table, 
    size_t     table_size, 
    void       **value // out parameter.
);

bool hash_allocate(
    size_t     num_entries, 
    hash_entry ***table
);

bool hash_free(
    hash_entry ***table, 
    size_t     table_size
);

#endif

