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
#include "orca.h"


static size_t update_write_callback(char *data, size_t size, size_t nmemb, void *userdata) {
	printf("update_write_callback: data=%p size=%llu nmemb=%llu\n", data, size, nmemb);
	oc_file *file = (oc_file *)userdata;
	return oc_file_write(*file, size * nmemb, data);
}

int update(int argc, char** argv)
{
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

	// download latest version of runtime
	// unzip it
	// rename orca-runtime-win to vX.X.X
	// download source code 
	// write new version to "current" file


	CURL *curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "Failed to initialize curl\n");
		return 1;
	}

	int result = 0;

	char errbuf[CURL_ERROR_SIZE] = {0};
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, update_write_callback);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // follow redirects

	oc_file file = oc_file_open(OC_STR8("RUNTIME.zip"), OC_FILE_ACCESS_WRITE, OC_FILE_OPEN_CREATE);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
	curl_easy_setopt(curl, CURLOPT_URL, 
		"https://github.com/bitwitch/orca/releases/latest/download/orca-runtime-win.zip");
	CURLcode res = curl_easy_perform(curl);

	oc_file_close(file);

	if (res != CURLE_OK) {
		fprintf(stderr, "Curl request failed: %s\n", curl_easy_strerror(res));
		result = 1;
	}

	curl_easy_cleanup(curl);

	return result;
}

