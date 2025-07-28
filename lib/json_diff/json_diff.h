#ifndef JSON_DIFF_H
#define JSON_DIFF_H

#include "../cJSON/cJSON.h"

/**
 * Compare two JSON files containing MD5 hashes and generate diff/same files
 * 
 * @param file1_path Path to the first JSON file
 * @param file2_path Path to the second JSON file
 * @param diff_output_path Output path for diff.json (files with different/unique hashes)
 * @param same_output_path Output path for same.json (files with same hashes)
 * @return 0 on success, -1 on error
 */
int compare_json_files(const char *file1_path, const char *file2_path, 
                      const char *diff_output_path, const char *same_output_path);

/**
 * Load and parse a JSON file
 * 
 * @param filepath Path to the JSON file
 * @return Parsed cJSON object or NULL on error
 */
cJSON *load_json_file(const char *filepath);

/**
 * Create a hash map from JSON files array for quick lookup
 * 
 * @param files_array cJSON array containing file objects with path and md5
 * @return Hash map structure or NULL on error
 */
typedef struct hash_entry {
    char *md5;
    char *path;
    struct hash_entry *next;
} hash_entry_t;

typedef struct {
    hash_entry_t **buckets;
    int size;
} hash_map_t;

hash_map_t *create_hash_map(int size);
void free_hash_map(hash_map_t *map);
int hash_map_insert(hash_map_t *map, const char *md5, const char *path);
hash_entry_t *hash_map_find(hash_map_t *map, const char *md5);

#endif // JSON_DIFF_H
