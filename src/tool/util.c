/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include "orca.h"
#include "util.h"

oc_str8 system_orca_dir(oc_arena* a)
{
    oc_str8 exe_path = oc_path_executable(a);
    return oc_path_slice_directory(exe_path);
}

oc_str8 current_sdk_version(oc_arena* a, bool fail_if_not_found)
{
    oc_str8 orca_dir = system_orca_dir(a);

    oc_str8 current_file_path = oc_path_append(a, orca_dir, OC_STR8("current_version"));
    oc_file file = oc_file_open(current_file_path, OC_FILE_ACCESS_READ, OC_FILE_OPEN_NONE);
    if (oc_file_is_nil(file)) {
        if(fail_if_not_found)
        {
            fprintf(stderr, "Failed to determine current Orca SDK version.\n");
            exit(1);
        }
        return (oc_str8){0};
    } 

    oc_str8 current_version = {0};
    current_version.len = oc_file_size(file);
    current_version.ptr = oc_arena_push(a, current_version.len + 1);
    oc_file_read(file, current_version.len, current_version.ptr);
    oc_io_error err = oc_file_last_error(file);
    oc_file_close(file);
    if(err != OC_IO_OK) 
    {
        if(fail_if_not_found)
        {
            fprintf(stderr, "Failed to determine current Orca SDK version.\n");
            exit(1);
        }
        return (oc_str8){0};
    }

    return oc_str8_trim_space(current_version);
}

oc_str8 current_version_dir(oc_arena* a)
{
    oc_str8 current_version = current_sdk_version(a, true);
    oc_str8 orca_dir = system_orca_dir(a);
    return oc_path_append(a, orca_dir, current_version);
}

bool isspace_cheap(int c)
{
    switch(c)
    {
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
        case '\v':
            return true;
        default:
            return false;
    }
}

oc_str8 oc_str8_trim_space(oc_str8 s)
{
    u64 start, end;
    for(u64 i = 0; i < s.len; i++)
    {
        start = i;
        if(!isspace_cheap(s.ptr[i]))
        {
            break;
        }
    }
    for(u64 i = s.len; i > 0; i--)
    {
        end = i;
        if(!isspace_cheap(s.ptr[i - 1]))
        {
            break;
        }
    }

    if(end <= start)
    {
        return OC_STR8("");
    }
    return oc_str8_slice(s, start, end);
}

bool oc_str8_ends_with(oc_str8 s, oc_str8 ending) {
	if (ending.len > s.len) return false;

	for (size_t i = 0; i < ending.len; ++i) {
		if (s.ptr[s.len - i] != ending.ptr[ending.len - i]) {
			return false;
		}
	}
	return true;
}




