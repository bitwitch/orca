/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>

#include "flag.h"

#include "orca.c"
#include "util.c"

#include "sdk_path.c"
#include "bundle.c"
#include "system.c"

int version(int argc, char** argv);

int main(int argc, char** argv)
{
    Flag_Context c;
    flag_init_context(&c);

    bool* doSdkPath = flag_command(&c, "sdk-path", "Print the path to the installed Orca SDK.");
    bool* doBundle = flag_command(&c, "bundle", "Package a WebAssembly module into a standalone Orca application.");
    bool* doVersion = flag_command(&c, "version", "Print the current Orca version.");

    // Hacks to achieve the following:
    // - `orca` => `orca -h`
    // - `orca -v`, `orca --version` => `orca version`
    if(argc == 1)
    {
        flag_print_usage(&c, "orca", stderr);
        return 0;
    }
    if(strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
    {
        argv[1] = "version";
    }

    if(!flag_parse(&c, argc, argv))
    {
        flag_print_usage(&c, "orca", stderr);
        if(flag_error_is_help(&c))
        {
            exit(0);
        }
        flag_print_error(&c, stderr);
        return 1;
    }

    if(!flag_parse_command(&c))
    {
        flag_print_usage(&c, "orca", stderr);
        flag_print_error(&c, stderr);
        return 1;
    }

    int rest_argc = flag_rest_argc(&c);
    char** rest_argv = flag_rest_argv(&c);

    if(*doSdkPath)
    {
        return sdkPath(rest_argc, rest_argv);
    }
    else if(*doBundle)
    {
        return bundle(rest_argc, rest_argv);
    }
    else if(*doVersion)
    {
        return version(rest_argc, rest_argv);
    }
    else
    {
        fprintf(stderr, "ERROR: didn't handle all available commands or something, o no\n");
        return 1;
    }

    return 0;
}

#ifndef ORCA_TOOL_VERSION
    #define ORCA_TOOL_VERSION unknown
#endif

// I love C so much
#define _TOSTRING(x) #x
#define TOSTRING(x) _TOSTRING(x)

int version(int argc, char** argv)
{
    fprintf(stderr, "Orca CLI tool version: ");
    printf(TOSTRING(ORCA_TOOL_VERSION));
    fprintf(stderr, "\n");

    // TODO: Print runtime / install version info

    return 0;
}
