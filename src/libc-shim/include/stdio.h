#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// same layout as oc_file to be used interchangably
struct FILE
{
    u64 h;
};
typedef struct FILE FILE;

FILE* fopen(const char* restrict name, const char* restrict type);
size_t fread(void* restrict buffer, size_t size, size_t n, FILE* restrict stream);
size_t fwrite(const void* buffer, size_t size, size_t n, FILE* restrict stream);
int feof(FILE* stream);
int ferror(FILE* stream);
int fclose(FILE* stream);

#ifdef __cplusplus
}
#endif

#endif
