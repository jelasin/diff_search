#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include "lib/calc_md5/calc_md5.h"
#include "lib/list_file/list_file.h"
#include "lib/cJSON/cJSON.h"

typedef struct {
    cJSON *json_array;
    int file_count;
    int error_count;
} process_context_t;

void process_file(const char *filepath, void *user_data) {
    process_context_t *ctx = (process_context_t *)user_data;
    char md5_string[33];
    
    printf("Processing: %s\n", filepath);
    
    if (calculate_file_md5(filepath, md5_string) == 0) {
        // Create JSON object for this file
        cJSON *file_obj = cJSON_CreateObject();
        cJSON *path_item = cJSON_CreateString(filepath);
        cJSON *md5_item = cJSON_CreateString(md5_string);
        
        cJSON_AddItemToObject(file_obj, "path", path_item);
        cJSON_AddItemToObject(file_obj, "md5", md5_item);
        
        // Add to array
        cJSON_AddItemToArray(ctx->json_array, file_obj);
        ctx->file_count++;
        
        printf("  MD5: %s\n", md5_string);
    } else {
        fprintf(stderr, "Error calculating MD5 for file: %s\n", filepath);
        ctx->error_count++;
    }
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <directory>\n", program_name);
    printf("Calculate MD5 checksums for all files in a directory tree and output as JSON.\n\n");
    printf("Options:\n");
    printf("  -o <file>    Output JSON to file (default: stdout)\n");
    printf("  -h           Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s /home/user/documents\n", program_name);
    printf("  %s -o checksums.json /home/user/documents\n", program_name);
}

int main(int argc, char *argv[]) {
    char *directory = NULL;
    char *output_file = NULL;
    int opt;
    
    // Parse command line arguments
    while ((opt = getopt(argc, argv, "o:h")) != -1) {
        switch (opt) {
            case 'o':
                output_file = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Get directory argument
    if (optind >= argc) {
        fprintf(stderr, "Error: Directory path is required.\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    directory = argv[optind];
    
    // Check if directory exists
    if (!is_directory(directory)) {
        fprintf(stderr, "Error: '%s' is not a valid directory.\n", directory);
        return 1;
    }
    
    printf("MD5 Directory Scanner\n");
    printf("====================\n");
    printf("Scanning directory: %s\n", directory);
    if (output_file) {
        printf("Output file: %s\n", output_file);
    } else {
        printf("Output: stdout\n");
    }
    printf("\n");
    
    // Initialize JSON structure
    cJSON *root = cJSON_CreateObject();
    cJSON *info = cJSON_CreateObject();
    cJSON *files_array = cJSON_CreateArray();
    
    // Add metadata
    char *abs_dir = get_absolute_path(directory);
    cJSON_AddStringToObject(info, "scanned_directory", abs_dir ? abs_dir : directory);
    
    // Add timestamp
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    // Remove newline from ctime result
    if (time_str && strlen(time_str) > 0) {
        time_str[strlen(time_str) - 1] = '\0';
    }
    cJSON_AddStringToObject(info, "scan_time", time_str);
    
    cJSON_AddItemToObject(root, "scan_info", info);
    cJSON_AddItemToObject(root, "files", files_array);
    
    // Initialize processing context
    process_context_t ctx = {
        .json_array = files_array,
        .file_count = 0,
        .error_count = 0
    };
    
    // Traverse directory and process files
    printf("Scanning files...\n\n");
    if (traverse_directory(directory, process_file, &ctx) != 0) {
        fprintf(stderr, "Error traversing directory.\n");
        cJSON_Delete(root);
        if (abs_dir) free(abs_dir);
        return 1;
    }
    
    // Add file count to metadata
    cJSON_AddNumberToObject(info, "total_files", ctx.file_count);
    cJSON_AddNumberToObject(info, "errors", ctx.error_count);
    
    // Output JSON
    char *json_string = cJSON_Print(root);
    if (!json_string) {
        fprintf(stderr, "Error generating JSON output.\n");
        cJSON_Delete(root);
        if (abs_dir) free(abs_dir);
        return 1;
    }
    
    if (output_file) {
        FILE *outfile = fopen(output_file, "w");
        if (!outfile) {
            fprintf(stderr, "Error opening output file: %s\n", output_file);
            free(json_string);
            cJSON_Delete(root);
            if (abs_dir) free(abs_dir);
            return 1;
        }
        fprintf(outfile, "%s\n", json_string);
        fclose(outfile);
        printf("\nResults written to: %s\n", output_file);
    } else {
        printf("\n=== JSON Output ===\n");
        printf("%s\n", json_string);
    }
    
    printf("\nScan complete!\n");
    printf("Files processed: %d\n", ctx.file_count);
    if (ctx.error_count > 0) {
        printf("Errors encountered: %d\n", ctx.error_count);
    }
    
    // Cleanup
    free(json_string);
    cJSON_Delete(root);
    if (abs_dir) free(abs_dir);
    
    return 0;
}
