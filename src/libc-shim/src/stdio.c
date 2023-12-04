/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <platform/platform_io.h>

static_assert(sizeof(FILE) == sizeof(oc_file));

///////////////////////////////////////////////////////////////////////////////////////////////////
// Local helpers

int oc_io_err_to_errno(oc_io_error_enum error)
{
	switch (error)
	{
		case OC_IO_OK: return 0;
		case OC_IO_ERR_UNKNOWN: return ;
		case OC_IO_ERR_OP: return EOPNOTSUPP;
		case OC_IO_ERR_HANDLE: return EINVAL;
		case OC_IO_ERR_PREV: return EINVAL;
		case OC_IO_ERR_ARG: return EINVAL;
		case OC_IO_ERR_PERM: return EPERM;
		case OC_IO_ERR_SPACE: return ENOSPC;
		case OC_IO_ERR_NO_ENTRY: return ENOENT;
		case OC_IO_ERR_EXISTS: return EEXIST;
		case OC_IO_ERR_NOT_DIR: return ENOTDIR;
		case OC_IO_ERR_DIR: return EISDIR;
		case OC_IO_ERR_MAX_FILES: return EMFILE;
		case OC_IO_ERR_MAX_LINKS: return ELOOP;
		case OC_IO_ERR_PATH_LENGTH: return ENAMETOOLONG;
		case OC_IO_ERR_FILE_SIZE: return EFBIG;
		case OC_IO_ERR_OVERFLOW: return EOVERFLOW;
		case OC_IO_ERR_NOT_READY: return EAGAIN;
		case OC_IO_ERR_MEM: return ENOMEM;
		case OC_IO_ERR_INTERRUPT: return EINTR;
		case OC_IO_ERR_PHYSICAL: return EIO;
		case OC_IO_ERR_NO_DEVICE: return ENXIO;
		case OC_IO_ERR_WALKOUT: return EPERM;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Public API

FILE* fopen(const char* restrict name, const char* restrict type)
{
	oc_file_access rights = 0;
	oc_file_open_flags flags = OC_FILE_OPEN_RESTRICT;
    for (const char* typeIter = type; *type; ++type)
    {
    	switch (*typeIter)
    	{
    	case 'r':
    		rights |= OC_FILE_ACCESS_READ;
    		break;

    	case 'w':
    		if (flags & OC_FILE_OPEN_APPEND)
    		{
    			return NULL;
    		}
    		rights |= OC_FILE_ACCESS_WRITE;
    		flags |= OC_FILE_OPEN_CREATE;
    		break;

    	case 'a':
    		if (flags & OC_FILE_OPEN_CREATE)
    		{
    			return NULL;
    		}
    		rights |= OC_FILE_ACCESS_WRITE;
    		flags |= OC_FILE_OPEN_APPEND;
    		break;

    	case '+':
    		rights |= OC_FILE_ACCESS_READ;
			rights |= OC_FILE_ACCESS_WRITE;
    		break;

	   	// NOTE: type specifiers 't' and 'b' are ignored - Orca doesn't do any special conversions for text files
    	case 't':
    		break;
    	case 'b':
    		break;

    	default:
    		// Invalid character
    		return NULL;
    	}
    }

	oc_file file = oc_file_open(OC_STR8(name), rights, flags);
	if (oc_io_error error = oc_file_last_error(file))
	{
		errno = oc_io_err_to_errno((oc_io_error_enum)error);
		oc_file_close(file);
		return NULL;
	}

	FILE* c_file = malloc(sizeof(FILE));
	c_file.h = file.h;

	return c_file;
}

size_t fread(void* restrict buffer, size_t size, size_t n, FILE* restrict stream)
{
	oc_file file {.h = stream->h};
	u64 bytes = oc_file_read(file, size * n, buffer);
	if (oc_io_error error = oc_file_last_error(file))
	{
		errno = oc_io_err_to_errno((oc_io_error_enum)error);
	}
	size_t clampedBytes = (bytes > SIZE_MAX) ? SIZE_MAX : (size_t)bytes;
	return clampedBytes;
}

size_t fwrite(const void* buffer, size_t size, size_t n, FILE* restrict stream)
{
	oc_file file {.h = stream->h};
	u64 bytes = oc_file_write(file, size * n, (char*)buffer);
	if (oc_io_error error = oc_file_last_error(file))
	{
		errno = oc_io_err_to_errno((oc_io_error_enum)error);
	}
	size_t clampedBytes = (bytes > SIZE_MAX) ? SIZE_MAX : (size_t)bytes;
	return clampedBytes;
}

int feof(FILE* stream)
{
	oc_file file {.h = stream->h};

	i64 pos = oc_file_pos(file);
	u64 size = oc_file_size(file);

	u64 upos = (pos > 0) ? (u64)pos : 0;
	return upos == size;
}

int ferror(FILE* stream)
{
	oc_file file {.h = stream->h};
	oc_io_error error = oc_file_last_error(file);
	return error != OC_IO_OK;
}

int fclose(FILE* stream)
{
	oc_file file = {.h = stream->h};
	if (oc_io_error error = oc_file_last_error(file))
	{
		errno = oc_io_err_to_errno((oc_io_error_enum)error);
	}
	oc_file_close(file);
	free(stream);
	return 0;
}
