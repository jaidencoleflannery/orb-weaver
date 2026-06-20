#ifndef HASH_TOOLS_H
#define HASH_TOOLS_H

#define MAX_KEY_LENGTH 256
#define KEYGEN_SALT 31

typedef struct { 
    void *value;
    char *key;
} hash_entry;

bool hash_allocate(size_t num_entries, hash_entry **result);

bool hash_free(hash_entry *key);

#endif

