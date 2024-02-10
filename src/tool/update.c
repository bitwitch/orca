/*************************************************************************
*
*  Orca
*  Copyright 2024 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include <stdio.h>

#include "curl/curl.h"
#include "flag.h"
#include "util.h"
#include "tarball.h"
#include "orca.h"

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

	// make a request to releases/latest and follow redirects to get the actual release url
	// get the version number from the realease url?
	// check if we already have this version, and if so just print "up to date"
	// download latest version of runtime
	// unzip it
	// rename orca-runtime-win to vX.X.X
	// download source code 
	// write new version current_versions.txt
	// append version number and checksum to all_checksums.txt

	/*
	orca
		orca.exe
		current_version.txt (file containing vX.X.X)
		all_checksums.txt
		vX.X.X
			bin
				orca_runtime.exe
			orca-libc
				orca_libc.wasm
			lib
				...
			resources
				font.ttf
			src
				(full source code)
	 */

	int result = 0;

	CURL *curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "error: failed to initialize curl\n");
		return 1;
	}

	//-----------------------------------------------------------------------------
	// get the latest version number from github release url
	//-----------------------------------------------------------------------------
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1); // follow redirects
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	curl_easy_setopt(curl, CURLOPT_URL, "https://github.com/bitwitch/orca/releases/latest");
	CURLcode curl_code = curl_easy_perform(curl);
	if (curl_code != CURLE_OK) {
		fprintf(stderr, "error: curl request failed: %s\n", curl_easy_strerror(curl_code));
		return 1;
	}

	char *last_url_cstr = NULL;
	curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &last_url_cstr);
	oc_str8 version = oc_path_slice_filename(OC_STR8(last_url_cstr));
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
	// TODO(shaw): use orca main repo instead of my fork repo
	oc_str8 source_url = oc_str8_pushf(&arena, 
		"https://github.com/bitwitch/orca/archive/refs/tags/%.*s.tar.gz", 
		oc_str8_printf(version));
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

	oc_str8 old_dir = oc_str8_pushf(&arena, "orca-%.*s", oc_str8_printf(version));
	old_dir = oc_path_append(&arena, temp_dir, old_dir);
	oc_str8 new_dir = oc_path_append(&arena, version_dir, OC_STR8("src"));

	if (!oc_sys_move(old_dir, new_dir)) {
		fprintf(stderr, "error: failed to move %s to %s\n", old_dir.ptr, new_dir.ptr);
		return 1;
	}

	//-----------------------------------------------------------------------------
	// download and extract latest runtime
	//-----------------------------------------------------------------------------
	// TODO(shaw): use orca main repo instead of my fork repo
	// oc_str8 runtime_url = OC_STR8("https://github.com/bitwitch/orca/releases/latest/download/orca-runtime-win.tar.gz");
	oc_str8 runtime_url = OC_STR8("https://github.com/bitwitch/orca/releases/latest/download/orca-runtime-mac-x64.tar.gz");
	oc_str8 runtime_filename = oc_path_slice_filename(runtime_url);
	oc_str8 runtime_filepath = oc_path_append(&arena, temp_dir, runtime_filename);

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

	// NOTE(shaw): assuming that the cli tool will always just call update and
	// exit so no cleanup is done, i.e. curl_easy_cleanup(curl)

	return 0;
}

