/*************************************************************************
*
*  Orca
*  Copyright 2024 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include <stdio.h>

#include "curl.h"
#include "flag.h"
#include "util.h"
#include "tarball.h"
#include "orca.h"

#if OC_PLATFORM_WINDOWS
        #define RUNTIME_FILENAME OC_STR8("orca-runtime-win.tar.gz")
        #define RUNTIME_EXE_PATH OC_STR8("orca-runtime-win/bin/orca_runtime.exe")
        #define ORCA_DLL_PATH    OC_STR8("orca-runtime-win/bin/orca.dll")
#elif OC_PLATFORM_MACOS
    #if OC_ARCH_ARM64
        #define RUNTIME_FILENAME OC_STR8("orca-runtime-mac-arm64.tar.gz")
        #define RUNTIME_EXE_PATH OC_STR8("orca-runtime-mac-arm64/bin/orca_runtime")
        #define ORCA_DLL_PATH    OC_STR8("orca-runtime-mac-arm64/bin/liborca.dylib")
    #else
        #define RUNTIME_FILENAME OC_STR8("orca-runtime-mac-x64.tar.gz")
        #define RUNTIME_EXE_PATH OC_STR8("orca-runtime-mac-x64/bin/orca_runtime")
        #define ORCA_DLL_PATH    OC_STR8("orca-runtime-mac-x64/bin/liborca.dylib")
    #endif
#else
	#error Unsupported platform
#endif

static size_t curl_callback_write_to_file(char *data, size_t size, size_t nmemb, void *userdata) 
{
	oc_file *file = (oc_file *)userdata;
	return oc_file_write(*file, size * nmemb, data);
}

static char curl_errbuf[CURL_ERROR_SIZE]; // buffer for last curl error message 

static char *curl_last_error(CURLcode code) 
{
	// if there is no message in curl_errbuf, then fall back to the less
	// detailed error message from the CURLcode
	u64 len = strlen(curl_errbuf);
	return len ? curl_errbuf : curl_easy_strerror(code);
}

static CURLcode download_file(CURL *handle, oc_str8 url, oc_str8 out_path) 
{
	oc_file file = oc_file_open(out_path, OC_FILE_ACCESS_WRITE, OC_FILE_OPEN_CREATE);
	if (oc_file_is_nil(file)) {
		oc_file_close(file);
		return CURLE_WRITE_ERROR;
	}

	curl_easy_reset(handle);
	curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, curl_errbuf);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1); 
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_callback_write_to_file);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &file);
	curl_easy_setopt(handle, CURLOPT_URL, url.ptr);
	CURLcode err = curl_easy_perform(handle);
	oc_file_close(file);
	return err;
}

int update(int argc, char** argv)
{
	oc_arena arena;
	oc_arena_init(&arena);

    Flag_Context c;
    flag_init_context(&c);

    flag_help(&c, "Downloads and installs the latest version of Orca.");

    if(!flag_parse(&c, argc, argv))
    {
        flag_print_usage(&c, "orca update", stderr);
        if(flag_error_is_help(&c))
        {
            return 0;
        }
        flag_print_error(&c, stderr);
        return 1;
    }

	CURL *curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "error: failed to initialize curl\n");
		return 1;
	}


	// TODO(shaw): use main orca repo instead of my fork
	oc_str8 repo_url_base = OC_STR8("https://github.com/bitwitch/orca");

	//-----------------------------------------------------------------------------
	// get the latest version number from github release url
	//-----------------------------------------------------------------------------
	oc_str8 latest_url = oc_path_append(&arena, repo_url_base, OC_STR8("/releases/latest"));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1); // follow redirects
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	curl_easy_setopt(curl, CURLOPT_URL, latest_url.ptr);
	CURLcode curl_code = curl_easy_perform(curl);
	if (curl_code != CURLE_OK) {
		fprintf(stderr, "error: curl request failed: %s\n", curl_easy_strerror(curl_code));
		return 1;
	}

	char *final_url = NULL;
	curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &final_url);
	oc_str8 version = oc_path_slice_filename(OC_STR8(final_url));
	oc_str8 exe_path = oc_path_executable(&arena);
	oc_str8 orca_dir = oc_path_slice_directory(exe_path);

    oc_str8 version_dir = oc_path_append(&arena, orca_dir, version);

	if (oc_sys_exists(version_dir)) {
		printf("Already up to date with version %.*s\n", oc_str8_printf(version));
		return 0;
	} 

	//-----------------------------------------------------------------------------
	// create directory structure for new version
	//-----------------------------------------------------------------------------
	oc_str8 bin_dir = oc_path_append(&arena, version_dir, OC_STR8("bin"));
	oc_str8 orca_libc_dir = oc_path_append(&arena, version_dir, OC_STR8("orca-libc"));
	oc_str8 temp_dir = oc_path_append(&arena, version_dir, OC_STR8("temporary"));

	TRY(oc_sys_mkdirs(version_dir));
	TRY(oc_sys_mkdirs(bin_dir));
	TRY(oc_sys_mkdirs(orca_libc_dir));
	TRY(oc_sys_mkdirs(temp_dir));

	//-----------------------------------------------------------------------------
	// download and extract latest source code
	//-----------------------------------------------------------------------------
	{
		oc_str8 source_url = oc_str8_pushf(&arena, "/archive/refs/tags/%.*s.tar.gz", 
			oc_str8_printf(version));
		source_url = oc_path_append(&arena, repo_url_base, source_url);
		oc_str8 source_filename = oc_path_slice_filename(source_url);
		oc_str8 source_filepath = oc_path_append(&arena, temp_dir, source_filename);

		curl_code = download_file(curl, source_url, source_filepath);
		if (curl_code != CURLE_OK) {
			fprintf(stderr, "error: failed to download file %s: %s\n", 
				source_url.ptr, curl_last_error(curl_code));
			return 1;
		}

		if (!tarball_extract(source_filepath, temp_dir)) {
			fprintf(stderr, "error: failed to extract files from %.*s\n", 
				oc_str8_printf(source_filepath));
			return 1;
		}

		// NOTE(shaw): github annoyingly removes 'v' from the directory name if the
		// tag is of the form vX.X.X
		oc_str8 repo_dir = {0};
		if (version.ptr[0] == 'v') {
			oc_str8 name = oc_str8_pushf(&arena, "orca-%.*s", version.len - 1, version.ptr + 1);
			repo_dir = oc_path_append(&arena, temp_dir, name);
		} else {
			oc_str8 name = oc_str8_pushf(&arena, "orca-%.*s", oc_str8_printf(version));
			repo_dir = oc_path_append(&arena, temp_dir, name);
		}

		oc_str8 repo_src_dir = oc_path_append(&arena, repo_dir, OC_STR8("src"));
		oc_str8 repo_resources_dir = oc_path_append(&arena, repo_dir, OC_STR8("resources"));
		oc_str8 src_dir = oc_path_append(&arena, version_dir, OC_STR8("src"));
		oc_str8 resources_dir = oc_path_append(&arena, version_dir, OC_STR8("resources"));

		TRY(oc_sys_move(repo_src_dir, src_dir));
		TRY(oc_sys_move(repo_resources_dir, resources_dir));
	}

	//-----------------------------------------------------------------------------
	// download and extract latest runtime
	//-----------------------------------------------------------------------------
	{
		oc_str8 runtime_url = oc_str8_pushf(&arena, "/releases/latest/download/%.*s", 
			oc_str8_printf(RUNTIME_FILENAME));
		runtime_url = oc_path_append(&arena, repo_url_base, runtime_url);
		oc_str8 runtime_filepath = oc_path_append(&arena, temp_dir, RUNTIME_FILENAME);

		curl_code = download_file(curl, runtime_url, runtime_filepath);
		if (curl_code != CURLE_OK) {
			fprintf(stderr, "error: failed to download file %s: %s\n", 
				runtime_url.ptr, curl_last_error(curl_code));
			return 1;
		}

		if (!tarball_extract(runtime_filepath, temp_dir)) {
			fprintf(stderr, "error: failed to extract files from %s\n", runtime_filepath.ptr);
			return 1;
		}

		oc_str8 orca_exe = oc_path_append(&arena, temp_dir, RUNTIME_EXE_PATH);
		oc_str8 orca_dll = oc_path_append(&arena, temp_dir, ORCA_DLL_PATH);
		TRY(oc_sys_move(orca_exe, bin_dir));
		TRY(oc_sys_move(orca_dll, bin_dir));
	}

	//-----------------------------------------------------------------------------
	// record checksum and update current_version file
	//-----------------------------------------------------------------------------

	// TODO(shaw): generate a checksum of the version directory and write to all_versions.txt
	/*
	{
		oc_str8 all_versions = oc_path_append(&arena, orca_dir, OC_STR8("all_versions.txt"));
		oc_file_open_flags open_flags = oc_sys_exists(all_versions) 
			? OC_FILE_OPEN_APPEND : OC_FILE_OPEN_CREATE;
		oc_file file = oc_file_open(all_versions, OC_FILE_ACCESS_WRITE, open_flags);
		if (!oc_file_is_nil(file)) {
			oc_file_seek(file, 0, OC_FILE_SEEK_END);
			oc_str8 checksum = get_directory_checksum(version_dir);
			oc_str8 version_and_checksum = oc_str8_pushf(&arena, "%.*s %.*s\n", 
					oc_str8_printf(version), oc_str8_printf(checksum));
			oc_file_write(file, version_and_checksum.len, version_and_checksum.ptr);
		} else {
			fprintf(stderr, "error: failed to open file %s\n", all_versions.ptr);
		}
		oc_file_close(file);
	}
	*/

	{
		oc_str8 current_version = oc_path_append(&arena, orca_dir, OC_STR8("current_version.txt"));
		oc_file file = oc_file_open(current_version, OC_FILE_ACCESS_WRITE, OC_FILE_OPEN_CREATE);
		if (!oc_file_is_nil(file)) {
			oc_file_write(file, version.len, version.ptr);
		} else {
			fprintf(stderr, "error: failed to open file %s\n", current_version.ptr);
		}
		oc_file_close(file);
	}

	TRY(oc_sys_rmdir(temp_dir));

	// NOTE(shaw): assuming that the cli tool will always just call update and
	// exit so no cleanup is done, i.e. curl_easy_cleanup(curl)

	return 0;
}

