#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#define __NEED_FILE
#define __NEED___isoc_va_list
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_off_t
#define __NEED_va_list

#include <bits/alltypes.h>

#define __need_NULL
#include <stddef.h>

#undef EOF
#define EOF (-1)

#undef SEEK_SET
#undef SEEK_CUR
#undef SEEK_END
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define BUFSIZ 1024
#define FILENAME_MAX 4096
#define FOPEN_MAX 1000

typedef union _G_fpos64_t
{
    char __opaque[16];
    long long __lldata;
    double __align;
} fpos_t;

char* fgets(char* restrict str, int num, FILE* restrict stream );
FILE* fopen(const char* restrict name, const char* restrict type);
FILE* freopen(const char* restrict filename, const char* restrict mode, FILE* restrict f);
int fclose(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
int fflush(FILE* stream);
int fgetc(FILE* stream);
int fgetpos(FILE* restrict stream, fpos_t* restrict pos);
int fputc(int character, FILE* stream);
int fputs(const char* restrict str, FILE* restrict stream );
int fseek(FILE* stream, long int offset, int origin);
int fsetpos(FILE* restrict stream, const fpos_t* pos);
int setvbuf(FILE* restrict f, char* restrict buf, int type, size_t size);
int ungetc(int c, FILE* f);
long int ftell(FILE* stream);
size_t fread(void* restrict buffer, size_t size, size_t n, FILE* restrict stream);
size_t fwrite(const void* restrict buffer, size_t size, size_t n, FILE* restrict stream);
void clearerr(FILE *f);
void rewind(FILE* stream);
void setbuf(FILE* restrict f, char* restrict buf);

#define putc fputc
#define getc fgetc

int sprintf(char* __restrict, const char* __restrict, ...);
int snprintf(char* __restrict, size_t, const char* __restrict, ...);

int vsprintf(char* __restrict, const char* __restrict, __isoc_va_list);
int vsnprintf(char* __restrict, size_t, const char* __restrict, __isoc_va_list);
int fprintf(FILE* restrict f, const char* restrict fmt, ...);
int vfprintf(FILE* restrict f, const char* restrict fmt, va_list ap);

int sscanf(const char* __restrict, const char* __restrict, ...);
int vsscanf(const char* __restrict, const char* __restrict, __isoc_va_list);
int fscanf(FILE* restrict f, const char* restrict fmt, ...);
int vfscanf(FILE* restrict f, const char* restrict fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif
