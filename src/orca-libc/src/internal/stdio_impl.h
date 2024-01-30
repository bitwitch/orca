#ifndef _STDIO_IMPL_H
#define _STDIO_IMPL_H

#include <stdio.h>

#define UNGET 8

#if defined(_REENTRANT)
    #define FFINALLOCK(f) ((f)->lock >= 0 ? __lockfile((f)) : 0)
    #define FLOCK(f) int __need_unlock = ((f)->lock >= 0 ? __lockfile((f)) : 0)
    #define FUNLOCK(f)             \
        do                         \
        {                          \
            if(__need_unlock)      \
                __unlockfile((f)); \
        }                          \
        while(0)
#else
    // No locking needed.
    #define FFINALLOCK(f) ((void)(f))
    #define FLOCK(f) ((void)(f))
    #define FUNLOCK(f) ((void)(f))
#endif

#define F_PERM 1
#define F_NORD 4
#define F_NOWR 8
#define F_EOF 16
#define F_ERR 32
#define F_SVB 64
#define F_APP 128

struct _IO_FILE
{
    long long unsigned orca_file;
    unsigned flags;
    unsigned char *rpos, *rend;
    int (*close)(FILE*);
    unsigned char *wend, *wpos;

    unsigned char* wbase;
    size_t (*read)(FILE*, unsigned char*, size_t);
    size_t (*write)(FILE*, const unsigned char*, size_t);
    off_t (*seek)(FILE*, off_t, int);
    unsigned char* buf;
    size_t buf_size;
    FILE *prev, *next;

#if defined(_REENTRANT)
    long lockcount;
#endif
    int mode;
#if defined(_REENTRANT)
    volatile int lock;
#endif
    int lbf;
    void* cookie;
    off_t off;
    char* getln_buf;

    unsigned char* shend;
    off_t shlim, shcnt;
#if defined(_REENTRANT)
    FILE *prev_locked, *next_locked;
#endif
    struct __locale_struct* locale;
};

extern hidden FILE* volatile __stdin_used;
extern hidden FILE* volatile __stdout_used;
extern hidden FILE* volatile __stderr_used;

#if defined(_REENTRANT)
hidden int __lockfile(FILE*);
hidden void __unlockfile(FILE*);
#endif

hidden size_t __file_read_err_shim(FILE* stream, unsigned char* buffer, size_t size);
hidden size_t __file_write_err_shim(FILE* stream, const unsigned char* buffer, size_t size);
hidden off_t __file_seek_err_shim(FILE* stream, off_t offset, int origin);
hidden int __file_close_err_shim(FILE* stream);

hidden int __toread(FILE*);
hidden int __towrite(FILE*);

hidden void __stdio_exit(void);
hidden void __stdio_exit_needed(void);

int __overflow(FILE*, int), __uflow(FILE*);

hidden int __fseeko(FILE*, off_t, int);
hidden int __fseeko_unlocked(FILE*, off_t, int);
hidden off_t __ftello(FILE*);
hidden off_t __ftello_unlocked(FILE*);
hidden size_t __fwritex(const unsigned char*, size_t, FILE*);
hidden int __putc_unlocked(int, FILE*);

hidden FILE* __fdopen(long long unsigned, const char*);
hidden int __fmodeflags(const char*);

hidden FILE* __ofl_add(FILE* f);
hidden FILE** __ofl_lock(void);
hidden void __ofl_unlock(void);

struct __pthread;
hidden void __register_locked_file(FILE*, struct __pthread*);
hidden void __unlist_locked_file(FILE*);
hidden void __do_orphaned_stdio_locks(void);

#define MAYBE_WAITERS 0x40000000

hidden void __getopt_msg(const char*, const char*, const char*, size_t);

#define getc_unlocked(f) \
    (((f)->rpos != (f)->rend) ? *(f)->rpos++ : __uflow((f)))

#define putc_unlocked(c, f)                                       \
    ((((unsigned char)(c) != (f)->lbf && (f)->wpos != (f)->wend)) \
         ? *(f)->wpos++ = (unsigned char)(c)                      \
         : __overflow((f), (unsigned char)(c)))

/* Caller-allocated FILE * operations */
hidden FILE* __fopen_rb_ca(const char*, FILE*, unsigned char*, size_t);
hidden int __fclose_ca(FILE*);

// Functions not exposed to orca but used with dummy FILE structs
hidden int vsscanf(const char* restrict s, const char* restrict fmt, va_list ap);
hidden int vfscanf(FILE* restrict f, const char* restrict fmt, va_list ap);
#endif
