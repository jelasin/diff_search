#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include "lib/calc_md5/calc_md5.h"
#include "lib/list_file/list_file.h"
#include "lib/cJSON/cJSON.h"
#include "lib/json_diff/json_diff.h"

typedef struct {
    cJSON *json_array;
    int file_count;
    int error_count;
    const char *base_directory;
} process_context_t;

// Calculate relative path from base directory
char* get_relative_path(const char *full_path, const char *base_path) {
    if (!full_path || !base_path) return NULL;
    
    // Find the length of the base path
    size_t base_len = strlen(base_path);
    
    // If full_path starts with base_path, return the relative part
    if (strncmp(full_path, base_path, base_len) == 0) {
        const char *relative_start = full_path + base_len;
        
        // Skip leading slash if present
        if (*relative_start == '/') {
            relative_start++;
        }
        
        // If the relative path is empty, return "."
        if (*relative_start == '\0') {
            return strdup(".");
        }
        
        return strdup(relative_start);
    }
    
    // If not under base path, return the full path
    return strdup(full_path);
}

void process_file(const char *filepath, void *user_data) {
    process_context_t *ctx = (process_context_t *)user_data;
    char md5_string[33];
    
    printf("Processing: %s\n", filepath);
    
    if (calculate_file_md5(filepath, md5_string) == 0) {
        // Calculate relative path
        char *relative_path = get_relative_path(filepath, ctx->base_directory);
        if (!relative_path) {
            fprintf(stderr, "Error calculating relative path for: %s\n", filepath);
            ctx->error_count++;
            return;
        }
        
        // Create JSON object for this file
        cJSON *file_obj = cJSON_CreateObject();
        cJSON *path_item = cJSON_CreateString(relative_path);
        cJSON *md5_item = cJSON_CreateString(md5_string);
        
        cJSON_AddItemToObject(file_obj, "path", path_item);
        cJSON_AddItemToObject(file_obj, "md5", md5_item);
        
        // Add to array
        cJSON_AddItemToArray(ctx->json_array, file_obj);
        ctx->file_count++;
        
        printf("  Relative path: %s\n", relative_path);
        printf("  MD5: %s\n", md5_string);
        
        free(relative_path);
    } else {
        fprintf(stderr, "Error calculating MD5 for file: %s\n", filepath);
        ctx->error_count++;
    }
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <directory>\n", program_name);
    printf("       %s --diff <file1.json> <file2.json>\n", program_name);
    printf("       %s --same <file1.json> <file2.json>\n", program_name);
    printf("Calculate MD5 checksums for all files in a directory tree and output as JSON,\n");
    printf("or compare two JSON files to find differences and similarities.\n\n");
    printf("Scan Mode Options:\n");
    printf("  -o <file>    Output JSON to file (default: stdout)\n");
    printf("  -h           Show this help message\n\n");
    printf("Compare Mode Options:\n");
    printf("  --diff       Compare two JSON files and output differences to diff.json\n");
    printf("  --same       Compare two JSON files and output similarities to same.json\n");
    printf("  --both       Compare two JSON files and output both diff.json and same.json\n\n");
    printf("Examples:\n");
    printf("  Scan directory:\n");
    printf("    %s /home/user/documents\n", program_name);
    printf("    %s -o checksums.json /home/user/documents\n", program_name);
    printf("  Compare files:\n");
    printf("    %s --diff file1.json file2.json\n", program_name);
    printf("    %s --same file1.json file2.json\n", program_name);
    printf("    %s --both file1.json file2.json\n", program_name);
}

int main(int argc, char *argv[]) {
    char *directory = NULL;
    char *output_file = NULL;
    int opt;
    int mode_diff = 0;
    int mode_same = 0;
    int mode_both = 0;
    
    // Define long options
    static struct option long_options[] = {
        {"diff", no_argument, 0, 'd'},
        {"same", no_argument, 0, 's'},
        {"both", no_argument, 0, 'b'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Parse command line arguments
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "o:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'o':
                output_file = optarg;
                break;
            case 'd':
                mode_diff = 1;
                break;
            case 's':
                mode_same = 1;
                break;
            case 'b':
                mode_both = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Check for comparison mode
    if (mode_diff || mode_same || mode_both) {
        // Comparison mode - need exactly 2 file arguments
        if (optind + 2 != argc) {
            fprintf(stderr, "Error: Comparison mode requires exactly 2 JSON files.\n\n");
            print_usage(argv[0]);
            return 1;
        }
        
        const char *file1 = argv[optind];
        const char *file2 = argv[optind + 1];
        
        const char *diff_output = "diff.json";
        const char *same_output = "same.json";
        
        if (mode_both || mode_diff) {
            if (mode_both || mode_same) {
                // Both diff and same, or just both
                if (compare_json_files(file1, file2, diff_output, same_output) != 0) {
                    fprintf(stderr, "Error: Failed to compare JSON files.\n");
                    return 1;
                }
            } else {
                // Only diff
                if (compare_json_files(file1, file2, diff_output, "/dev/null") != 0) {
                    fprintf(stderr, "Error: Failed to compare JSON files.\n");
                    return 1;
                }
                printf("Differences saved to: %s\n", diff_output);
            }
        } else if (mode_same) {
            // Only same
            if (compare_json_files(file1, file2, "/dev/null", same_output) != 0) {
                fprintf(stderr, "Error: Failed to compare JSON files.\n");
                return 1;
            }
            printf("Similarities saved to: %s\n", same_output);
        }
        
        return 0;
    }
    
    // Scan mode - original functionality
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
        .error_count = 0,
        .base_directory = abs_dir ? abs_dir : directory
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
