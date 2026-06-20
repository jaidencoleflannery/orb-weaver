#ifndef HASH_TOOLS_H
#define HASH_TOOLS_H

#define MAX_KEY_LENGTH

typedef struct {
    char *key;
    void *value;
    size_t index; // hash index.
} hash_entry;

bool hash_allocate(size_t num_entries, hash_entry **result);

bool hash_free(hash_entry *key);

#endif

