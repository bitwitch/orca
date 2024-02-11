/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#if OC_PLATFORM_WINDOWS
    #include <combaseapi.h>
    #include <knownfolders.h>
    #include <shlobj_core.h>
    #include <winerror.h>
#endif

#include "orca.h"
#include "flag.h"
#include "util.h"

oc_str8 get_current_version_dir(oc_arena* a)
{
	oc_str8 exe_path = oc_path_executable(a);
	oc_str8 orca_dir = oc_path_slice_directory(exe_path);

    oc_str8 current_file_path = oc_path_append(a, orca_dir, OC_STR8("current_version.txt"));
	oc_file file = oc_file_open(current_file_path, OC_FILE_ACCESS_READ, OC_FILE_OPEN_NONE);
	if (oc_file_is_nil(file)) {
        fprintf(stderr, "Failed to determine current Orca SDK version.\n");
        exit(1);
	} 

    char buf[64];
	oc_file_read(file, sizeof(buf), buf);
	oc_io_error err = oc_file_last_error(file);
	oc_file_close(file);
	if (err != OC_IO_OK) {
        fprintf(stderr, "Failed to determine current Orca SDK version.\n");
        exit(1);
	}

    oc_str8 current_version = OC_STR8(buf);
    current_version = oc_str8_trim_space(current_version);

    return oc_path_append(a, orca_dir, current_version);
}

int sdkPath(int argc, char** argv)
{
    oc_arena a;
    oc_arena_init(&a);

    Flag_Context c;
    flag_init_context(&c);

    flag_help(&c, "Prints the path to the installed Orca SDK. For use in scripts, e.g. `-I $(orca sdk-path)/src`.");

    // TODO: version selection

    if(!flag_parse(&c, argc, argv))
    {
        flag_print_usage(&c, "orca sdk-path", stderr);
        if(flag_error_is_help(&c))
        {
            return 0;
        }
        flag_print_error(&c, stderr);
        return 1;
    }

    oc_str8 sdk_dir = get_current_version_dir(&a);

    printf("%.*s", oc_str8_printf(sdk_dir));

    return 0;
}


