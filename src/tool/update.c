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

#if OC_PLATFORM_WINDOWS
        #define RELEASE_FILENAME OC_STR8("orca-win")
#elif OC_PLATFORM_MACOS
    #if OC_ARCH_ARM64
        #define RELEASE_FILENAME OC_STR8("orca-mac-arm64")
    #else
        #define RELEASE_FILENAME OC_STR8("orca-mac-x64")
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

	oc_str8 temp_dir = oc_path_append(&arena, orca_dir, OC_STR8("temporary"));
	if (oc_sys_exists(temp_dir)) {
		TRY(oc_sys_rmdir(temp_dir));
	}
	TRY(oc_sys_mkdirs(temp_dir));

	//-----------------------------------------------------------------------------
	// download and extract latest version
	//-----------------------------------------------------------------------------
	{
		oc_str8 release_tarname = oc_str8_pushf(&arena, "%.*s.tar.gz", 
			oc_str8_printf(RELEASE_FILENAME));
		oc_str8 release_url = oc_str8_pushf(&arena, "/releases/latest/download/%.*s", 
			oc_str8_printf(release_tarname));
		release_url = oc_path_append(&arena, repo_url_base, release_url);
		oc_str8 release_filepath = oc_path_append(&arena, temp_dir, release_tarname);

		curl_code = download_file(curl, release_url, release_filepath);
		if (curl_code != CURLE_OK) {
			fprintf(stderr, "error: failed to download file %s: %s\n", 
				release_url.ptr, curl_last_error(curl_code));
			return 1;
		}

		if (!tarball_extract(release_filepath, temp_dir)) {
			fprintf(stderr, "error: failed to extract files from %s\n", release_filepath.ptr);
			return 1;
		}

		oc_str8 extracted_release = oc_path_append(&arena, temp_dir, RELEASE_FILENAME);
		if (!oc_sys_move(extracted_release, version_dir)) {
			fprintf(stderr, "error: failed to move %s to %s\n", 
				extracted_release.ptr, version_dir.ptr);
			return 1;
		}
	}

	//-----------------------------------------------------------------------------
	// record checksum and update current_version file
	//-----------------------------------------------------------------------------

	{
		oc_str8 checksum = {0};
		oc_str8 checksum_path = oc_path_append(&arena, temp_dir, OC_STR8("sha1.sum"));
		oc_file checksum_file = oc_file_open(checksum_path, OC_FILE_ACCESS_READ, OC_FILE_OPEN_NONE);
		if (!oc_file_is_nil(checksum_file)) {
			checksum.len = oc_file_size(checksum_file);
			checksum.ptr = oc_arena_push(&arena, checksum.len + 1);
			oc_file_read(checksum_file, checksum.len, checksum.ptr);
		}
		oc_file_close(checksum_file);

		if (checksum.len) {
			oc_str8 all_versions = oc_path_append(&arena, orca_dir, OC_STR8("all_versions.txt"));
			oc_file_open_flags open_flags = oc_sys_exists(all_versions) 
				? OC_FILE_OPEN_APPEND : OC_FILE_OPEN_CREATE;
			oc_file file = oc_file_open(all_versions, OC_FILE_ACCESS_WRITE, open_flags);
			if (!oc_file_is_nil(file)) {
				oc_file_seek(file, 0, OC_FILE_SEEK_END);
				oc_str8 version_and_checksum = oc_str8_pushf(&arena, "%.*s %.*s\n", 
						oc_str8_printf(version), oc_str8_printf(checksum));
				oc_file_write(file, version_and_checksum.len, version_and_checksum.ptr);
			} else {
				fprintf(stderr, "error: failed to open file %s\n", all_versions.ptr);
			}
			oc_file_close(file);
		}
	}

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

	// TRY(oc_sys_rmdir(temp_dir));

	// NOTE(shaw): assuming that the cli tool will always just call update and
	// exit so no cleanup is done, i.e. curl_easy_cleanup(curl)

	return 0;
}

