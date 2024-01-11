// #ifdef __wasilibc_unmodified_upstream // WASI has no syscall
// #else
// #include <unistd.h>
// #include <wasi/libc.h>
// #endif
#include "stdio_impl.h"
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// #include <errno.h>
// #include <assert.h>
#include <util/typedefs.h>
#include <platform/platform_io.h>

int oc_io_err_to_errno(enum oc_io_error_enum error)
{
	switch(error)
	{
		case OC_IO_OK:
			return 0;
		case OC_IO_ERR_UNKNOWN:
			return EINVAL;
		case OC_IO_ERR_OP:
			return EOPNOTSUPP;
		case OC_IO_ERR_HANDLE:
			return EINVAL;
		case OC_IO_ERR_PREV:
			return EINVAL;
		case OC_IO_ERR_ARG:
			return EINVAL;
		case OC_IO_ERR_PERM:
			return EPERM;
		case OC_IO_ERR_SPACE:
			return ENOSPC;
		case OC_IO_ERR_NO_ENTRY:
			return ENOENT;
		case OC_IO_ERR_EXISTS:
			return EEXIST;
		case OC_IO_ERR_NOT_DIR:
			return ENOTDIR;
		case OC_IO_ERR_DIR:
			return EISDIR;
		case OC_IO_ERR_MAX_FILES:
			return EMFILE;
		case OC_IO_ERR_MAX_LINKS:
			return ELOOP;
		case OC_IO_ERR_PATH_LENGTH:
			return ENAMETOOLONG;
		case OC_IO_ERR_FILE_SIZE:
			return EFBIG;
		case OC_IO_ERR_OVERFLOW:
			return EOVERFLOW;
		case OC_IO_ERR_NOT_READY:
			return EAGAIN;
		case OC_IO_ERR_MEM:
			return ENOMEM;
		case OC_IO_ERR_INTERRUPT:
			return EINTR;
		case OC_IO_ERR_PHYSICAL:
			return EIO;
		case OC_IO_ERR_NO_DEVICE:
			return ENXIO;
		case OC_IO_ERR_WALKOUT:
			return EPERM;
	}
	return 0;
}

static size_t file_read_shim(FILE* stream, unsigned char* buffer, size_t size)
{
	oc_file file = { .h = stream->orca_file };

	// if (f->buf_size > 0)
	// {

	// }

	// In the wasilibc __stdio_read(), there is this behavior that writes the last character of len
	// to the file buffer, and to the output stream as well. This code preserves that behavior as
	// it is needed by __uflow(). See original source at:
	// https://github.com/WebAssembly/wasi-libc/blob/main/libc-top-half/musl/src/stdio/__stdio_read.c
	char* buffers[2] = { (char*)buffer, (char*)stream->buf };
	u64 lengths[2] = { size - !!stream->buf_size, stream->buf_size };
	u64 read_bytes[2] = {0};


	for (int i = 0; i < 2; ++i)
	{
		if (lengths[i])
		{
			read_bytes[i] = oc_file_read(file, lengths[i], buffers[i]);

			oc_io_error error = oc_file_last_error(file);
			if (error != OC_IO_OK)
			{
				errno = oc_io_err_to_errno(error);
				stream->flags |= F_ERR;
				return 0;
			}
		}	
	}

	u64 total_read = read_bytes[0] + read_bytes[1];

	if (total_read == 0)
	{
		stream->flags |= F_EOF;
		return 0;
	}

	if (total_read <= lengths[0])
	{
		return total_read;
	}

	u64 stream_buffer_size = read_bytes[1];

	stream->rpos = stream->buf;
	stream->rend = stream->buf + stream_buffer_size;
	if (stream->buf_size)
	{
		buffer[size - 1] = *stream->rpos;
		++stream->rpos;
	}
	return size;
}

static size_t file_write_shim(FILE* stream, const unsigned char* buffer, size_t size)
{
	oc_file file = { .h = stream->orca_file };

	// write out any data in the internal buffer, then the requested buffer
	char* buffers[2] = { (char*)stream->wbase, (char*)buffer };
	u64 lengths[2] = { stream->wpos - stream->wbase, size };

	for (int i = 0; i < 2; ++i)
	{
		u64 total = 0;
		while (total < lengths[i])
		{
			u64 written = oc_file_write(file, lengths[i] - total, buffers[i] + total);

			oc_io_error error = oc_file_last_error(file);
			if(error != OC_IO_OK)
			{
				errno = oc_io_err_to_errno(error);
				stream->flags |= F_ERR;
				stream->wbase = 0;
				stream->wpos = 0;
				stream->wend = 0;
				return -1;
			}

			total += written;
		}
	}

	// reset the internal buffer now that all the data has been written
	stream->wend = stream->buf + stream->buf_size;
	stream->wbase = stream->buf;
	stream->wpos = stream->wbase;
	return size;
}

static off_t file_seek_shim(FILE* stream, off_t offset, int origin)
{
	static const oc_file_whence LIBC_WHENCE_TO_OC_WHENCE[3] = {
		OC_FILE_SEEK_CURRENT, // SEEK_SET
		OC_FILE_SEEK_SET, // SEEK_CUR
		OC_FILE_SEEK_END, // SEEK_END
	};
	oc_file_whence whence = LIBC_WHENCE_TO_OC_WHENCE[origin];

	oc_file file = { .h = stream->orca_file };

	i64 result = oc_file_seek(file, offset, whence);
	oc_io_error error = oc_file_last_error(file);
	if(error != OC_IO_OK)
	{
		errno = oc_io_err_to_errno(error);
		stream->flags |= F_ERR;
		result = -1;
	}

	return result;
}

static int file_close_shim(FILE* stream)
{
	oc_file file = { .h = stream->orca_file };
	oc_file_close(file);
	return 0;
}

FILE *fopen(const char *restrict filename, const char *restrict mode)
{
	// FILE *f;
	// int fd;
	// int flags;

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return NULL;
	}

	/* Compute the flags to pass to open() */
	int flags = __fmodeflags(mode);

// int __fmodeflags(const char *mode)
// {
// 	int flags;
// 	if (strchr(mode, '+')) flags = O_RDWR;
// 	else if (*mode == 'r') flags = O_RDONLY;
// 	else flags = O_WRONLY;
// 	if (strchr(mode, 'x')) flags |= O_EXCL;
// 	if (strchr(mode, 'e')) flags |= O_CLOEXEC;
// 	if (*mode != 'r') flags |= O_CREAT;
// 	if (*mode == 'w') flags |= O_TRUNC;
// 	if (*mode == 'a') flags |= O_APPEND;
// 	return flags;
// }

	
	// fd = __wasilibc_open_nomode(filename, flags);

	// (ORCA): 
	oc_file_access orca_rights = 0;
	if (flags & O_RDWR)
	{
		orca_rights = OC_FILE_ACCESS_READ | OC_FILE_ACCESS_WRITE;
	}
	else if (flags & O_WRONLY)
	{
		orca_rights = OC_FILE_ACCESS_WRITE;
	}
	else // if (flags & O_RDONLY) - O_RDONLY is 0
	{
		orca_rights = OC_FILE_ACCESS_READ;
	}

	oc_file_open_flags orca_flags = OC_FILE_OPEN_RESTRICT;

	if (flags & O_CREAT)
	{
		orca_flags |= OC_FILE_OPEN_CREATE;
	}
	if (flags & O_TRUNC)
	{
		orca_flags |= OC_FILE_OPEN_TRUNCATE;
	}
	if (flags & O_APPEND)
	{
		orca_flags |= OC_FILE_OPEN_APPEND;		
	}

	oc_file file = oc_file_open(OC_STR8(filename), orca_rights, orca_flags);
	oc_io_error error = oc_file_last_error(file);
	if(error != OC_IO_OK)
	{
		errno = oc_io_err_to_errno(error);
		oc_file_close(file);
		return NULL;
	}

	// ORCA: original musl code called __fdopen to be resused for tmpfile(), but since we don't support that
	//       we can just inline the FILE struct setup here.

	FILE *f;
// #ifdef __wasilibc_unmodified_upstream // WASI has no syscall
// 	struct winsize wsz;
// #endif

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	/* Allocate FILE+buffer or fail */
	if (!(f=malloc(sizeof *f + UNGET + BUFSIZ))) return 0;

	/* Zero-fill only the struct, not the buffer */
	memset(f, 0, sizeof *f);

	/* Impose mode restrictions */
	if (!strchr(mode, '+')) f->flags = (*mode == 'r') ? F_NOWR : F_NORD;

	/* Apply close-on-exec flag */
// #ifdef __wasilibc_unmodified_upstream // WASI has no syscall
// 	if (strchr(mode, 'e')) __syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);
// #else
	// if (strchr(mode, 'e')) fcntl(fd, F_SETFD, FD_CLOEXEC);
// #endif

	/* Set append mode on fd if opened for append */
	if (*mode == 'a') {
// #ifdef __wasilibc_unmodified_upstream // WASI has no syscall
// 		int flags = __syscall(SYS_fcntl, fd, F_GETFL);
// #else
		// int flags = fcntl(fd, F_GETFL);
// #endif
		// if (!(flags & O_APPEND))
// #ifdef __wasilibc_unmodified_upstream // WASI has no syscall
// 			__syscall(SYS_fcntl, fd, F_SETFL, flags | O_APPEND);
// #else
			// fcntl(fd, F_SETFL, flags | O_APPEND);
// #endif
		f->flags |= F_APP;
	}

	f->orca_file = file.h;
	f->buf = (unsigned char *)f + sizeof *f + UNGET;
	f->buf_size = BUFSIZ;

	/* Activate line buffered mode for terminals */
	f->lbf = EOF;
// #ifdef __wasilibc_unmodified_upstream // WASI has no syscall
// 	if (!(f->flags & F_NOWR) && !__syscall(SYS_ioctl, fd, TIOCGWINSZ, &wsz))
// #else
	// if (!(f->flags & F_NOWR) && __isatty(fd))
// #endif
		// f->lbf = '\n';

	/* Initialize op ptrs. No problem if some are unneeded. */
	f->read = file_read_shim;
	f->write = file_write_shim;
	f->seek = file_seek_shim;
	f->close = file_close_shim;

#if defined(_REENTRANT)
	if (!libc.threaded) f->lock = -1;
#endif

	/* Add new FILE to open file list */
	return __ofl_add(f);

	// f = __fdopen(file.h, mode);
	// if (f) return f;

	// oc_file_close(file);
	// return NULL;

	// close(fd);
	// (ORCA)

	// return 0;
}

weak_alias(fopen, fopen64);
