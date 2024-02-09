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
		libcurl.dll
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
		fprintf(stderr, "Failed to initialize curl\n");
		return 1;
	}

	char errbuf[CURL_ERROR_SIZE] = {0};

	//-----------------------------------------------------------------------------
	// make a request to /releases/latest and follow redirects to get the
	// actual release url in order to find the current version number
	//-----------------------------------------------------------------------------
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1); // follow redirects
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	curl_easy_setopt(curl, CURLOPT_URL, "https://github.com/bitwitch/orca/releases/latest");
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "Curl request failed: %s\n", curl_easy_strerror(res));
		return 1;
	}

	char *last_url_cstr = NULL;
	curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &last_url_cstr);

	oc_str8 version = oc_path_slice_filename(OC_STR8(last_url_cstr));
	oc_str8 exe_path = oc_path_executable(&arena);
	oc_str8 orca_dir = oc_path_slice_directory(exe_path);
    oc_str8 version_dir_path = oc_path_append(&arena, orca_dir, version);

	if (oc_sys_exists(version_dir_path)) {
		printf("Already up to date with version %s\n", version.ptr);
		return 0;
	} 

	//-----------------------------------------------------------------------------
	// download latest version of runtime
	//-----------------------------------------------------------------------------
	// oc_str8 runtime_url = OC_STR8("https://github.com/bitwitch/orca/releases/latest/download/orca-runtime-win.tar.gz");
	oc_str8 runtime_url = OC_STR8("https://github.com/bitwitch/orca/releases/latest/download/orca-runtime-mac-x64.tar.gz");
	oc_str8 runtime_filename = oc_path_slice_filename(runtime_url);
	oc_file runtime_file = oc_file_open(runtime_filename, OC_FILE_ACCESS_WRITE, OC_FILE_OPEN_CREATE);
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1); 
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback_write_to_file);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &runtime_file);
	curl_easy_setopt(curl, CURLOPT_URL, runtime_url.ptr);
	res = curl_easy_perform(curl);
	oc_file_close(runtime_file);
	if (res != CURLE_OK) {
		fprintf(stderr, "Curl request failed: %s\n", curl_easy_strerror(res));
		return 1;
	}

	//-----------------------------------------------------------------------------
	// extract contents from the tarball
	//-----------------------------------------------------------------------------
	int rc = tarball_extract(runtime_filename);
	if (rc != 0) {
		fprintf(stderr, "error: failed to extract files from %s\n", runtime_filename.ptr);
		return 1;
	}

	// NOTE(shaw): assuming that the cli tool will always just call update and
	// exit so no cleanup is done, i.e. curl_easy_cleanup(curl)

	return 0;
}

