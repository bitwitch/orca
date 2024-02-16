/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#ifndef __SYSTEM_H_
#define __SYSTEM_H_

#include "orca.h"

#define OC_SYS_MAX_ERROR 1024

typedef struct
{
	char msg[OC_SYS_MAX_ERROR];
    i32 code;
} oc_sys_err_def;

extern oc_sys_err_def oc_sys_err;

oc_str8 oc_sys_getcwd(oc_arena* a);
bool oc_sys_exists(oc_str8 path);
bool oc_sys_isdir(oc_str8 path);
bool oc_sys_mkdirs(oc_str8 path);
bool oc_sys_rmdir(oc_str8 path);
bool oc_sys_copy(oc_str8 src, oc_str8 dst);
bool oc_sys_copytree(oc_str8 src, oc_str8 dst);
bool oc_sys_move(oc_str8 src, oc_str8 dst);

#define TRY(cmd)                                                                              \
    {                                                                                         \
        bool __result = cmd;                                                                  \
        if(!__result)                                                                         \
        {                                                                                     \
            int code = oc_sys_err.code;                                                       \
            if(code == 0)                                                                     \
            {                                                                                 \
                code = 1;                                                                     \
            }                                                                                 \
            fprintf(stderr, "ERROR (code %d): %s\n", code, oc_sys_err.msg); \
            return code;                                                                      \
        }                                                                                     \
    }

#endif // __SYSTEM_H_
