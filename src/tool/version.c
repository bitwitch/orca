/*************************************************************************
*
*  Orca
*  Copyright 2024 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "orca.h"
#include "flag.h"
#include "util.h"

int version(int argc, char** argv)
{
    oc_arena a;
    oc_arena_init(&a);

    Flag_Context c;
    flag_init_context(&c);

    flag_help(&c, "Prints the path to the installed Orca SDK. For use in scripts, e.g. `-I $(orca sdk-path)/src`.");

    // TODO: version selection

    if(!flag_parse(&c, argc, argv))
    {
        flag_print_usage(&c, "orca version", stderr);
        if(flag_error_is_help(&c))
        {
            return 0;
        }
        flag_print_error(&c, stderr);
        return 1;
    }

    oc_str8 version_dir = get_current_version_dir(&a);
	oc_str8 version = oc_path_slice_filename(version_dir);

    printf("Orca CLI tool version: %.*s\n", oc_str8_ip(version));

    return 0;
}

