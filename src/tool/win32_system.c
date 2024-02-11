/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include <processenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "orca.h"
#include "system.h"

oc_sys_err_def oc_sys_err;

// TODO: I guarantee I need to do more to escape paths EVERYWHERE.

oc_str8 oc_sys_getcwd(oc_arena* a)
{
    u64 len = GetCurrentDirectory(0, NULL);
    char* buf = oc_arena_push(a, len);
    GetCurrentDirectory(len, buf);
    return OC_STR8(buf);
}

bool oc_sys_exists(oc_str8 path)
{
    struct _stat stat;

    oc_arena_scope scratch = oc_scratch_begin();
    const char* cpath = oc_str8_to_cstring(scratch.arena, path);
    int result = _stat(cpath, &stat);
    oc_scratch_end(scratch);

    if(result)
    {
        OC_ASSERT(errno == ENOENT);
        return false;
    }

    return true;
}

bool oc_sys_isdir(oc_str8 path)
{
    struct _stat stat;

    oc_arena_scope scratch = oc_scratch_begin();
    const char* cpath = oc_str8_to_cstring(scratch.arena, path);
    int result = _stat(cpath, &stat);
    oc_scratch_end(scratch);

    if(result)
    {
        OC_ASSERT(errno == ENOENT);
        return false;
    }

    return (stat.st_mode & _S_IFDIR) != 0;
}

bool oc_sys_mkdirs(oc_str8 path)
{

	oc_arena_scope scratch = oc_scratch_begin();
	const char *cpath = oc_str8_to_cstring(scratch.arena, path);

	char path_buf[_MAX_PATH];
	char *abspath = _fullpath(path_buf, cpath, _MAX_PATH);

	oc_str8 cmd = oc_str8_pushf(scratch.arena, "mkdir \"%s\"", abspath);
    int result = system(cmd.ptr);
    oc_scratch_end(scratch);

    if(result)
    {
        oc_sys_err = (oc_sys_err_def){
            .msg = OC_STR8("failed to create directories"),
            .code = result,
        };
        return false;
    }

    return true;
}

bool oc_sys_rmdir(oc_str8 path)
{
    oc_arena_scope scratch = oc_scratch_begin();
    oc_str8 cmd = oc_str8_pushf(scratch.arena,
                                "rd /s /q \"%.*s\"",
                                oc_str8_printf(path));
    int result = system(cmd.ptr);
    oc_scratch_end(scratch);

    if(result)
    {
        oc_sys_err = (oc_sys_err_def){
            .msg = OC_STR8("failed to remove directory"),
            .code = result,
        };
        return false;
    }

    return true;
}

bool oc_sys_copy(oc_str8 src, oc_str8 dst)
{
    if(oc_sys_isdir(src))
    {
        oc_sys_err = (oc_sys_err_def){
            .msg = OC_STR8("can only copy files, not directories; use oc_sys_copytree for directories"),
        };
        return false;
    }

	oc_arena_scope scratch = oc_scratch_begin();
	char *csrc = oc_str8_to_cstring(scratch.arena, src);
	char *cdst = oc_str8_to_cstring(scratch.arena, dst);

	char src_buf[_MAX_PATH];
	char dst_buf[_MAX_PATH];
	char *full_src = _fullpath(src_buf, csrc, _MAX_PATH);
	char *full_dst = _fullpath(dst_buf, cdst, _MAX_PATH);

	oc_str8 cmd = oc_str8_pushf(scratch.arena, "copy \"%s\" \"%s\"", full_src, full_dst);
    int result = system(cmd.ptr);
    oc_scratch_end(scratch);

    if(result)
    {
        oc_sys_err = (oc_sys_err_def){
            .msg = OC_STR8("failed to copy file"),
            .code = result,
        };
        return false;
    }

    return true;
}

bool oc_sys_copytree(oc_str8 src, oc_str8 dst)
{
    if(!oc_sys_isdir(src))
    {
        oc_sys_err = (oc_sys_err_def){
            .msg = OC_STR8("can only copy directories, not files; use oc_sys_copy for files"),
        };
        return false;
    }

    oc_str8 dst_dir = oc_path_slice_directory(dst);
    if(!oc_sys_exists(dst_dir) && !oc_sys_mkdirs(dst_dir))
    {
        return false;
    }

	oc_arena_scope scratch = oc_scratch_begin();
	char *csrc = oc_str8_to_cstring(scratch.arena, src);
	char *cdst = oc_str8_to_cstring(scratch.arena, dst);

	char src_buf[_MAX_PATH];
	char dst_buf[_MAX_PATH];
	char *full_src = _fullpath(src_buf, csrc, _MAX_PATH);
	char *full_dst = _fullpath(dst_buf, cdst, _MAX_PATH);

	oc_str8 cmd = oc_str8_pushf(scratch.arena, "xcopy /s /e /y \"%s\" \"%s\"", full_src, full_dst);
    int result = system(cmd.ptr);
    oc_scratch_end(scratch);

    if(result)
    {
        oc_sys_err = (oc_sys_err_def){
            .msg = OC_STR8("failed to copy tree"),
            .code = result,
        };
        return false;
    }

    return true;
}

bool oc_sys_move(oc_str8 src, oc_str8 dst) 
{
	if(!oc_sys_exists(src)) 
	{
		oc_sys_err = (oc_sys_err_def){
			.msg = OC_STR8("failed to move file or directory, source does not exist"),
		};
		return false;
	}

	oc_arena_scope scratch = oc_scratch_begin();
	char *csrc = oc_str8_to_cstring(scratch.arena, src);
	char *cdst = oc_str8_to_cstring(scratch.arena, dst);

	char src_buf[_MAX_PATH];
	char dst_buf[_MAX_PATH];
	char *full_src = _fullpath(src_buf, csrc, _MAX_PATH);
	char *full_dst = _fullpath(dst_buf, cdst, _MAX_PATH);

	oc_str8 cmd = oc_str8_pushf(scratch.arena, "move \"%s\" \"%s\"", full_src, full_dst);
	int result = system(cmd.ptr);
	oc_scratch_end(scratch);

	if(result)
	{
		oc_sys_err = (oc_sys_err_def){
			.msg = OC_STR8("failed to move file or directory"),
				.code = result,
		};
		return false;
	}

	return true;
}
