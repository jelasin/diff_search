#define _GNU_SOURCE
#include "json_diff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HASH_MAP_SIZE 1024

static unsigned int hash_function(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_MAP_SIZE;
}

hash_map_t *create_hash_map(int size) {
    hash_map_t *map = malloc(sizeof(hash_map_t));
    if (!map) return NULL;
    
    map->size = size;
    map->buckets = calloc(size, sizeof(hash_entry_t*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }
    
    return map;
}

void free_hash_map(hash_map_t *map) {
    if (!map) return;
    
    for (int i = 0; i < map->size; i++) {
        hash_entry_t *entry = map->buckets[i];
        while (entry) {
            hash_entry_t *next = entry->next;
            free(entry->md5);
            free(entry->path);
            free(entry);
            entry = next;
        }
    }
    
    free(map->buckets);
    free(map);
}

int hash_map_insert(hash_map_t *map, const char *md5, const char *path) {
    if (!map || !md5 || !path) return -1;
    
    unsigned int index = hash_function(md5);
    hash_entry_t *entry = malloc(sizeof(hash_entry_t));
    if (!entry) return -1;
    
    entry->md5 = strdup(md5);
    entry->path = strdup(path);
    if (!entry->md5 || !entry->path) {
        free(entry->md5);
        free(entry->path);
        free(entry);
        return -1;
    }
    
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    
    return 0;
}

hash_entry_t *hash_map_find(hash_map_t *map, const char *md5) {
    if (!map || !md5) return NULL;
    
    unsigned int index = hash_function(md5);
    hash_entry_t *entry = map->buckets[index];
    
    while (entry) {
        if (strcmp(entry->md5, md5) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

cJSON *load_json_file(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filepath);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fprintf(stderr, "Error: Empty or invalid file %s\n", filepath);
        fclose(file);
        return NULL;
    }
    
    // Read file content
    char *content = malloc(file_size + 1);
    if (!content) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(content, 1, file_size, file);
    content[read_size] = '\0';
    fclose(file);
    
    // Parse JSON
    cJSON *json = cJSON_Parse(content);
    free(content);
    
    if (!json) {
        fprintf(stderr, "Error: Invalid JSON format in file %s\n", filepath);
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "JSON Error: %s\n", error_ptr);
        }
        return NULL;
    }
    
    return json;
}

int compare_json_files(const char *file1_path, const char *file2_path, 
                      const char *diff_output_path, const char *same_output_path) {
    if (!file1_path || !file2_path || !diff_output_path || !same_output_path) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }
    
    printf("Comparing JSON files:\n");
    printf("  File 1: %s\n", file1_path);
    printf("  File 2: %s\n", file2_path);
    printf("  Diff output: %s\n", diff_output_path);
    printf("  Same output: %s\n", same_output_path);
    printf("\n");
    
    // Load both JSON files
    cJSON *json1 = load_json_file(file1_path);
    if (!json1) return -1;
    
    cJSON *json2 = load_json_file(file2_path);
    if (!json2) {
        cJSON_Delete(json1);
        return -1;
    }
    
    // Get files arrays
    cJSON *files1 = cJSON_GetObjectItem(json1, "files");
    cJSON *files2 = cJSON_GetObjectItem(json2, "files");
    
    if (!cJSON_IsArray(files1) || !cJSON_IsArray(files2)) {
        fprintf(stderr, "Error: Invalid JSON structure - 'files' array not found\n");
        cJSON_Delete(json1);
        cJSON_Delete(json2);
        return -1;
    }
    
    // Create hash map for file2 for quick lookup
    hash_map_t *map2 = create_hash_map(HASH_MAP_SIZE);
    if (!map2) {
        fprintf(stderr, "Error: Failed to create hash map\n");
        cJSON_Delete(json1);
        cJSON_Delete(json2);
        return -1;
    }
    
    // Populate hash map with file2 data
    cJSON *file_item = NULL;
    cJSON_ArrayForEach(file_item, files2) {
        cJSON *path_item = cJSON_GetObjectItem(file_item, "path");
        cJSON *md5_item = cJSON_GetObjectItem(file_item, "md5");
        
        if (cJSON_IsString(path_item) && cJSON_IsString(md5_item)) {
            hash_map_insert(map2, md5_item->valuestring, path_item->valuestring);
        }
    }
    
    // Create output JSON structures
    cJSON *diff_root = cJSON_CreateObject();
    cJSON *same_root = cJSON_CreateObject();
    cJSON *diff_info = cJSON_CreateObject();
    cJSON *same_info = cJSON_CreateObject();
    cJSON *diff_files = cJSON_CreateArray();
    cJSON *same_files = cJSON_CreateArray();
    
    // Add metadata
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    if (time_str && strlen(time_str) > 0) {
        time_str[strlen(time_str) - 1] = '\0';
    }
    
    cJSON_AddStringToObject(diff_info, "comparison_time", time_str);
    cJSON_AddStringToObject(diff_info, "file1", file1_path);
    cJSON_AddStringToObject(diff_info, "file2", file2_path);
    cJSON_AddStringToObject(diff_info, "description", "Files with different or unique MD5 hashes");
    
    cJSON_AddStringToObject(same_info, "comparison_time", time_str);
    cJSON_AddStringToObject(same_info, "file1", file1_path);
    cJSON_AddStringToObject(same_info, "file2", file2_path);
    cJSON_AddStringToObject(same_info, "description", "Files with matching MD5 hashes");
    
    cJSON_AddItemToObject(diff_root, "comparison_info", diff_info);
    cJSON_AddItemToObject(diff_root, "files", diff_files);
    cJSON_AddItemToObject(same_root, "comparison_info", same_info);
    cJSON_AddItemToObject(same_root, "files", same_files);
    
    int same_count = 0;
    int diff_count = 0;
    
    // Process files from file1
    cJSON_ArrayForEach(file_item, files1) {
        cJSON *path_item = cJSON_GetObjectItem(file_item, "path");
        cJSON *md5_item = cJSON_GetObjectItem(file_item, "md5");
        
        if (cJSON_IsString(path_item) && cJSON_IsString(md5_item)) {
            const char *path1 = path_item->valuestring;
            const char *md5 = md5_item->valuestring;
            
            hash_entry_t *found = hash_map_find(map2, md5);
            if (found) {
                // Same hash found in both files
                cJSON *same_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(same_obj, "md5", md5);
                cJSON_AddStringToObject(same_obj, "file1_path", path1);
                cJSON_AddStringToObject(same_obj, "file2_path", found->path);
                cJSON_AddItemToArray(same_files, same_obj);
                same_count++;
            } else {
                // Hash only exists in file1
                cJSON *diff_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(diff_obj, "md5", md5);
                cJSON_AddStringToObject(diff_obj, "file1_path", path1);
                cJSON_AddStringToObject(diff_obj, "file2_path", "");
                cJSON_AddStringToObject(diff_obj, "status", "only_in_file1");
                cJSON_AddItemToArray(diff_files, diff_obj);
                diff_count++;
            }
        }
    }
    
    // Create another hash map for file1 to find hashes only in file2
    hash_map_t *map1 = create_hash_map(HASH_MAP_SIZE);
    if (!map1) {
        fprintf(stderr, "Error: Failed to create hash map for file1\n");
        free_hash_map(map2);
        cJSON_Delete(json1);
        cJSON_Delete(json2);
        cJSON_Delete(diff_root);
        cJSON_Delete(same_root);
        return -1;
    }
    
    // Populate hash map with file1 data
    cJSON_ArrayForEach(file_item, files1) {
        cJSON *path_item = cJSON_GetObjectItem(file_item, "path");
        cJSON *md5_item = cJSON_GetObjectItem(file_item, "md5");
        
        if (cJSON_IsString(path_item) && cJSON_IsString(md5_item)) {
            hash_map_insert(map1, md5_item->valuestring, path_item->valuestring);
        }
    }
    
    // Find hashes only in file2
    cJSON_ArrayForEach(file_item, files2) {
        cJSON *path_item = cJSON_GetObjectItem(file_item, "path");
        cJSON *md5_item = cJSON_GetObjectItem(file_item, "md5");
        
        if (cJSON_IsString(path_item) && cJSON_IsString(md5_item)) {
            const char *path2 = path_item->valuestring;
            const char *md5 = md5_item->valuestring;
            
            hash_entry_t *found = hash_map_find(map1, md5);
            if (!found) {
                // Hash only exists in file2
                cJSON *diff_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(diff_obj, "md5", md5);
                cJSON_AddStringToObject(diff_obj, "file1_path", "");
                cJSON_AddStringToObject(diff_obj, "file2_path", path2);
                cJSON_AddStringToObject(diff_obj, "status", "only_in_file2");
                cJSON_AddItemToArray(diff_files, diff_obj);
                diff_count++;
            }
        }
    }
    
    // Add counts to metadata
    cJSON_AddNumberToObject(diff_info, "total_differences", diff_count);
    cJSON_AddNumberToObject(same_info, "total_matches", same_count);
    
    // Write output files
    char *diff_json_string = cJSON_Print(diff_root);
    char *same_json_string = cJSON_Print(same_root);
    
    if (!diff_json_string || !same_json_string) {
        fprintf(stderr, "Error: Failed to generate JSON output\n");
        free_hash_map(map1);
        free_hash_map(map2);
        cJSON_Delete(json1);
        cJSON_Delete(json2);
        cJSON_Delete(diff_root);
        cJSON_Delete(same_root);
        return -1;
    }
    
    // Write diff file
    FILE *diff_file = fopen(diff_output_path, "w");
    if (!diff_file) {
        fprintf(stderr, "Error: Cannot create diff output file %s\n", diff_output_path);
        free(diff_json_string);
        free(same_json_string);
        free_hash_map(map1);
        free_hash_map(map2);
        cJSON_Delete(json1);
        cJSON_Delete(json2);
        cJSON_Delete(diff_root);
        cJSON_Delete(same_root);
        return -1;
    }
    fprintf(diff_file, "%s\n", diff_json_string);
    fclose(diff_file);
    
    // Write same file
    FILE *same_file = fopen(same_output_path, "w");
    if (!same_file) {
        fprintf(stderr, "Error: Cannot create same output file %s\n", same_output_path);
        free(diff_json_string);
        free(same_json_string);
        free_hash_map(map1);
        free_hash_map(map2);
        cJSON_Delete(json1);
        cJSON_Delete(json2);
        cJSON_Delete(diff_root);
        cJSON_Delete(same_root);
        return -1;
    }
    fprintf(same_file, "%s\n", same_json_string);
    fclose(same_file);
    
    printf("Comparison completed successfully!\n");
    printf("Files with same MD5: %d (saved to %s)\n", same_count, same_output_path);
    printf("Files with different/unique MD5: %d (saved to %s)\n", diff_count, diff_output_path);
    
    // Cleanup
    free(diff_json_string);
    free(same_json_string);
    free_hash_map(map1);
    free_hash_map(map2);
    cJSON_Delete(json1);
    cJSON_Delete(json2);
    cJSON_Delete(diff_root);
    cJSON_Delete(same_root);
    
    return 0;
}
