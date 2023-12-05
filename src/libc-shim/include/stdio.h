#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// same layout as oc_file to be used interchangably
struct FILE
{
    long long unsigned h;
};
typedef struct FILE FILE;

struct fpos_t
{
    long long int pos;
};
typedef struct fpos_t fpos_t;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

FILE* fopen(const char* restrict name, const char* restrict type);
size_t fread(void* restrict buffer, size_t size, size_t n, FILE* restrict stream);
size_t fwrite(const void* restrict buffer, size_t size, size_t n, FILE* restrict stream);
long int ftell(FILE* stream);
int fseek(FILE* stream, long int offset, int origin);
int fgetpos(FILE* restrict stream, fpos_t* restrict pos);
int fsetpos(FILE* restrict stream, const fpos_t* pos);
int fflush(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
int fclose(FILE* stream);

#ifdef __cplusplus
}
#endif

#endif // _STDIO_H
