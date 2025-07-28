#include "list_file.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int is_regular_file(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return 0;
    }
    return S_ISREG(statbuf.st_mode);
}

int is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

char *get_absolute_path(const char *path) {
    char *absolute_path = realpath(path, NULL);
    return absolute_path;
}

int traverse_directory(const char *dir_path, file_callback_t callback, void *user_data) {
    DIR *dir;
    struct dirent *entry;
    char full_path[PATH_MAX];
    
    dir = opendir(dir_path);
    if (dir == NULL) {
        fprintf(stderr, "Error opening directory %s: %s\n", dir_path, strerror(errno));
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Construct full path
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (is_regular_file(full_path)) {
            // Process regular file
            char *abs_path = get_absolute_path(full_path);
            if (abs_path) {
                callback(abs_path, user_data);
                free(abs_path);
            }
        } else if (is_directory(full_path)) {
            // Recursively traverse subdirectory
            traverse_directory(full_path, callback, user_data);
        }
    }
    
    closedir(dir);
    return 0;
}
