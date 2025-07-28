#ifndef LIST_FILE_H
#define LIST_FILE_H

#include <stdio.h>

// Callback function type for processing each file
typedef void (*file_callback_t)(const char *filepath, void *user_data);

// Function to recursively traverse directory and call callback for each file
int traverse_directory(const char *dir_path, file_callback_t callback, void *user_data);

// Check if a path is a regular file
int is_regular_file(const char *path);

// Check if a path is a directory
int is_directory(const char *path);

// Get absolute path of a file
char *get_absolute_path(const char *path);

#endif // LIST_FILE_H
