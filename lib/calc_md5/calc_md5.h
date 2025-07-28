#ifndef CALC_MD5_H
#define CALC_MD5_H

#include <stdint.h>
#include <stddef.h>

// MD5 context structure
typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} MD5_CTX;

// MD5 function declarations
void md5_init(MD5_CTX *ctx);
void md5_update(MD5_CTX *ctx, const uint8_t *data, size_t len);
void md5_final(MD5_CTX *ctx, uint8_t digest[16]);

// High-level function to calculate MD5 of a file
int calculate_file_md5(const char *filename, char *md5_string);

// Convert MD5 digest to hex string
void md5_to_string(const uint8_t digest[16], char *output);

#endif // CALC_MD5_H
