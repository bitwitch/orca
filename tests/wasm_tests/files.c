/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include <orca.h>

#include <stdio.h>
#include <errno.h>

int check_string(FILE* f, oc_str8 test_string)
{
    char buffer[256];
    size_t n = fread(buffer, 1, 256, f);
    if(ferror(f))
    {
        oc_log_error("Error while reading test string\n");
        return (-1);
    }

    if(oc_str8_cmp(test_string, oc_str8_from_buffer(n, buffer)))
    {
        return (-1);
    }

    return (0);
}

int test_read(void)
{
    {
        oc_str8 path = OC_STR8("regular.txt");
        FILE* f = fopen(path.ptr, "r");
        if(f == NULL || ferror(f))
        {
            oc_log_error("Can't open file %.*s for reading\n", (int)path.len, path.ptr);
            return (-1);
        }

        oc_str8 test_string = OC_STR8("Hello from regular.txt");
        if(check_string(f, test_string))
        {
            oc_log_error("Check string failed\n");
            return (-1);
        }

        fclose(f);
    }

    {
        oc_str8 path = OC_STR8("directory/test.txt");
        oc_str8 test_string = OC_STR8("Hello from directory/test.txt");

        FILE* f = fopen(path.ptr, "r");
        if(f == NULL || ferror(f))
        {
            oc_log_error("Can't open file %.*s for reading\n", (int)path.len, path.ptr);
            return (-1);
        }

        if(check_string(f, test_string))
        {
            oc_log_error("Check string failed\n");
            return (-1);
        }

        fclose(f);
    }

    {
        FILE* f = fopen("does_not_exist.txt", "r");
        if(f != NULL)
        {
            oc_log_error("Somehow opened a file that doesn't exist\n");
            return (-1);
        }
    }

    return (0);
}

int test_write(void)
{
    oc_arena_scope scratch = oc_scratch_begin();
    oc_arena* arena = scratch.arena;

    oc_str8 path = OC_STR8("write_test.txt");
    oc_str8 test_string = OC_STR8("Hello from write_test.txt");

    FILE* f = fopen(path.ptr, "w");
    if(ferror(f))
    {
        oc_log_error("Can't create/open file %.*s for writing\n", (int)path.len, path.ptr);
        return (-1);
    }

    size_t written = fwrite(test_string.ptr, 1, test_string.len, f);
    if(ferror(f))
    {
        oc_log_error("Error while writing %.*s\n", (int)path.len, path.ptr);
        return (-1);
    }
    if(written != test_string.len)
    {
        oc_log_error("Failed to write the entire string to file. written: %zu, expected: %zu", written, test_string.len);
        return (-1);
    }
    fclose(f);

    char* pathCStr = oc_str8_to_cstring(arena, path);
    FILE* file = fopen(pathCStr, "r");
    if(!file)
    {
        oc_log_error("File %.*s not found while checking\n", (int)path.len, path.ptr);
        return (-1);
    }
    char buffer[256];
    size_t n = fread(buffer, 1, 256, file);
    if(n != test_string.len || strncmp(buffer, test_string.ptr, test_string.len))
    {
        oc_log_error("Didn't recover test string\n");
        return (-1);
    }
    fclose(file);

    return (0);
}

int test_error(void)
{
    {
        FILE* f = fopen("regular.txt", "r");
        oc_str8 test_string = OC_STR8("this shouldn't get written since the file is in read mode");
        size_t written = fwrite(test_string.ptr, 1, test_string.len, f);
        if(!ferror(f))
        {
            oc_log_error("File should be in error state");
            return (-1);
        }
        if(written > 0)
        {
            oc_log_error("Wrote %d bytes but shouldn't have written any.", (int)written);
            return (-1);
        }

        clearerr(f);
        if(ferror(f))
        {
            oc_log_error("File error state should be clearec");
            return (-1);
        }
    }

    {
        FILE* f = fopen("regular.txt", "w");

        oc_str8 test_string = OC_STR8("Hello from regular.txt");
        char buffer[256];
        size_t n = fread(buffer, 1, sizeof(buffer), f);

        if(!ferror(f))
        {
            oc_log_error("File should be in error state");
            return (-1);
        }

        clearerr(f);
        if(ferror(f))
        {
            oc_log_error("File error state should be clearec");
            return (-1);
        }
    }

    return (0);
}

int test_seek(void)
{
    const char* filename = "temp_big_file.bin";
    FILE* f = fopen(filename, "w+");
    if(ferror(f))
    {
        return (-1);
    }

    // should only write 256 bytes of zeroes to the file since we're resetting the head position every time
    const unsigned char MAX_LOOPS = 0xFF;
    for(unsigned char i = 0; i < MAX_LOOPS; ++i)
    {
        unsigned char empty_data[16];
        memset(empty_data, i + 1, sizeof(empty_data));

        if(i > 0)
        {
            int err = fseek(f, -((long int)sizeof(empty_data)), SEEK_CUR);
            if(err)
            {
                oc_log_error("Failed to SEEK_CUR\n");
                return (-1);
            }
        }

        size_t n = fwrite(empty_data, 1, sizeof(empty_data), f);
        if(ferror(f))
        {
            oc_log_error("Caught error writing to %s\n", filename);
            return (-1);
        }
        if(n != sizeof(empty_data))
        {
            oc_log_error("Failed to write all bytes to %s\n", filename);
            return (-1);
        }
    }

    fflush(f);
    if(ferror(f))
    {
        oc_log_error("fflush failed: %d\n", errno);
        return (-1);
    }

    {
        int err = fseek(f, 0, SEEK_SET);
        if(err)
        {
            oc_log_error("Failed to SEEK_SET\n");
            return (-1);
        }

        unsigned char written_data[16];
        size_t n = fread(&written_data, 1, sizeof(written_data), f);
        if(ferror(f))
        {
            oc_log_error("Caught error reading data: %d\n", errno);
            return (-1);
        }

        if(n != sizeof(written_data))
        {
            oc_log_error("Failed to read enough data\n");
            return (-1);
        }

        for(i32 i = 0; i < sizeof(written_data) / sizeof(*written_data); ++i)
        {
            if(written_data[i] != MAX_LOOPS)
            {
                oc_log_error("Read %d but expected %d\n", written_data[i], MAX_LOOPS);
                return (-1);
            }
        }
    }

    {
        int err = fseek(f, 0, SEEK_END);
        if(err)
        {
            oc_log_error("Failed to SEEK_END\n");
            return (-1);
        }
    }

    fclose(f);

    return (0);
}

// int test_jail(void)
// {
//     oc_log_info("test jail\n");

//     FILE* fopen()
// }

// int test_jail(void)
// {
//     oc_log_info("test jail\n");

//     oc_file jail = oc_file_open(OC_STR8("./data/jail"), OC_FILE_ACCESS_READ, 0);
//     if(oc_file_last_error(jail))
//     {
//         oc_log_error("Can't open jail directory\n");
//         return (-1);
//     }

//     //-----------------------------------------------------------
//     //NOTE: Check escapes
//     //-----------------------------------------------------------
//     oc_log_info("check potential escapes\n");

//     //NOTE: escape with absolute path
//     oc_file f = oc_file_open_at(jail, OC_STR8("/tmp"), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) != OC_IO_ERR_NO_ENTRY)
//     {
//         oc_log_error("Escaped jail with absolute path /tmp\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //NOTE: escape with ..
//     f = oc_file_open_at(jail, OC_STR8(".."), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) != OC_IO_ERR_WALKOUT)
//     {
//         oc_log_error("Escaped jail with relative path ..\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //NOTE: escape with dir/../..
//     f = oc_file_open_at(jail, OC_STR8("dir/../.."), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) != OC_IO_ERR_WALKOUT)
//     {
//         oc_log_error("Escaped jail with relative path dir/../..\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //NOTE: escape with symlink to parent
//     f = oc_file_open_at(jail, OC_STR8("/dir_escape"), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) != OC_IO_ERR_WALKOUT)
//     {
//         oc_log_error("Escaped jail with symlink to parent\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //NOTE: escape to file with symlink to parent
//     f = oc_file_open_at(jail, OC_STR8("/dir_escape/regular.txt"), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) != OC_IO_ERR_WALKOUT)
//     {
//         oc_log_error("Escaped jail to regular.txt with symlink to parent\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //NOTE: escape with symlink to file
//     f = oc_file_open_at(jail, OC_STR8("/file_escape"), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) != OC_IO_ERR_WALKOUT)
//     {
//         oc_log_error("Escaped jail with symlink to file regular.txt\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //NOTE: escape with bad root handle
//     oc_file wrong_handle = { 0 };
//     f = oc_file_open_at(wrong_handle, OC_STR8("./data/regular.txt"), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) == OC_IO_OK)
//     {
//         oc_log_error("Escaped jail with nil root handle\n");
//         return (-1);
//     }
//     if(oc_file_last_error(f) != OC_IO_ERR_HANDLE)
//     {
//         oc_log_error("OC_FILE_OPEN_RESTRICT with invalid root handle should return OC_IO_ERR_HANDLE\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //-----------------------------------------------------------
//     //NOTE: empty path
//     //-----------------------------------------------------------
//     oc_log_info("check empty path\n");

//     f = oc_file_open_at(jail, OC_STR8(""), OC_FILE_ACCESS_READ, 0);
//     if(oc_file_last_error(f) != OC_IO_ERR_ARG)
//     {
//         oc_log_error("empty path should return OC_IO_ERR_ARG\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //-----------------------------------------------------------
//     //NOTE: Check legitimates open
//     //-----------------------------------------------------------
//     oc_log_info("check legitimates open\n");

//     //NOTE: regular file jail/test.txt
//     f = oc_file_open_at(jail, OC_STR8("/test.txt"), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) != OC_IO_OK)
//     {
//         oc_log_error("Can't open jail/test.txt\n");
//         return (-1);
//     }
//     if(check_string(f, OC_STR8("Hello from jail/test.txt")))
//     {
//         oc_log_error("Check string failed\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //NOTE: valid file traversal to jail/test.txt
//     f = oc_file_open_at(jail, OC_STR8("/dir/../test.txt"), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) != OC_IO_OK)
//     {
//         oc_log_error("Can't open jail/dir/../test.txt\n");
//         return (-1);
//     }
//     if(check_string(f, OC_STR8("Hello from jail/test.txt")))
//     {
//         oc_log_error("Check string failed\n");
//         return (-1);
//     }
//     oc_file_close(f);

//     //NOTE: re-open root directory
//     f = oc_file_open_at(jail, OC_STR8("."), OC_FILE_ACCESS_READ, OC_FILE_OPEN_RESTRICT);
//     if(oc_file_last_error(f) != OC_IO_OK)
//     {
//         oc_log_error("Can't open jail/.\n");
//         return (-1);
//     }
//     {
//         //NOTE: access regular file test.txt inside reopened root
//         oc_file f2 = oc_file_open_at(f, OC_STR8("test.txt"), OC_FILE_ACCESS_READ, 0);

//         if(check_string(f2, OC_STR8("Hello from jail/test.txt")))
//         {
//             oc_log_error("Check string failed\n");
//             return (-1);
//         }
//         oc_file_close(f2);
//     }
//     oc_file_close(f);

//     return (0);
// }

ORCA_EXPORT i32 oc_on_test(void)
{
    if(test_read())
    {
        return (-1);
    }
    if(test_write())
    {
        return (-1);
    }
    if(test_error())
    {
        return (-1);
    }
    if(test_seek())
    {
        return (-1);
    }
    // if(test_jail())
    // {
    //     return (-1);
    // }

    oc_log_info("OK\n");

    return (0);
}
