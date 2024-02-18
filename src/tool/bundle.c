/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

//#include <processenv.h>
#include <stdio.h>

#include "flag.h"
#include "orca.h"
#include "system.h"

int winBundle(
    oc_arena* a,
    oc_str8 name,
    oc_str8 icon,
    oc_str8 version,
    oc_str8_list resource_files,
    oc_str8_list resource_dirs,
    oc_str8 outDir,
    oc_str8 module);

int macBundle(
    oc_arena* a,
    oc_str8 name,
    oc_str8 icon,
    oc_str8 version,
    oc_str8_list resource_files,
    oc_str8_list resource_dirs,
    oc_str8 outDir,
    oc_str8 module,
	bool mtlEnableCapture);

int bundle(int argc, char** argv)
{
    oc_arena a;
    oc_arena_init(&a);

    Flag_Context c;
    flag_init_context(&c);

    flag_help(&c, "Packages a WebAssembly module into a standalone Orca application, along with any required assets.");

    char** name = flag_str(&c, "n", "name", "out", "the app's name");
    char** icon = flag_str(&c, "i", "icon", NULL, "an image file to use as the application's icon");
    char** version = flag_str(&c, NULL, "version", "0.0.0", "a version number to embed in the application bundle");
    oc_str8_list* resource_files = flag_strs(&c, "d", "resource", "copy a file to the app's resource directory");
    oc_str8_list* resource_dirs = flag_strs(&c, "D", "resource-dir", "copy the contents of a folder to the app's resource directory");
    char** outDir = flag_str(&c, "C", "out-dir", NULL, "where to place the final application bundle (defaults to the current directory)");
    bool* mtlEnableCapture = flag_bool(&c, "M", "mtl-enable-capture", false, "analyze your app's performance by invoking Metal's frame capture");

    char** module = flag_pos(&c, "module", "a .wasm file containing the application's wasm module");

    if(!flag_parse(&c, argc, argv))
    {
        flag_print_usage(&c, "orca bundle", stderr);
        if(flag_error_is_help(&c))
        {
            return 0;
        }
        flag_print_error(&c, stderr);
        return 1;
    }

    if(!flag_parse_positional(&c))
    {
        flag_print_usage(&c, "orca bundle", stderr);
        flag_print_error(&c, stderr);
        return 1;
    }


#if OC_PLATFORM_WINDOWS
    return winBundle(
        &a,
        OC_STR8(*name),
        OC_STR8(*icon),
        OC_STR8(*version),
        *resource_files,
        *resource_dirs,
        OC_STR8(*outDir),
        OC_STR8(*module));
#elif OC_PLATFORM_MACOS
    return macBundle(
        &a,
        OC_STR8(*name),
        OC_STR8(*icon),
        OC_STR8(*version),
        *resource_files,
        *resource_dirs,
        OC_STR8(*outDir),
        OC_STR8(*module),
		*mtlEnableCapture);
#else
    #error Can't build the bundle script on this platform!
#endif
}

#if OC_PLATFORM_WINDOWS

int winBundle(
    oc_arena* a,
    oc_str8 name,
    oc_str8 icon,
    oc_str8 version,
    oc_str8_list resource_files,
    oc_str8_list resource_dirs,
    oc_str8 outDir,
    oc_str8 module)
{
    //-----------------------------------------------------------
    //NOTE: make bundle directory structure
    //-----------------------------------------------------------
	oc_str8 orcaDir = get_current_version_dir(a);
    oc_str8 bundleDir = oc_path_append(a, outDir, name);
    oc_str8 exeDir = oc_path_append(a, bundleDir, OC_STR8("bin"));
    oc_str8 resDir = oc_path_append(a, bundleDir, OC_STR8("resources"));
    oc_str8 guestDir = oc_path_append(a, bundleDir, OC_STR8("app"));
    oc_str8 wasmDir = oc_path_append(a, guestDir, OC_STR8("wasm"));
    oc_str8 dataDir = oc_path_append(a, guestDir, OC_STR8("data"));

    if(oc_sys_exists(bundleDir))
    {
        TRY(oc_sys_rmdir(bundleDir));
    }
    TRY(oc_sys_mkdirs(bundleDir));
    TRY(oc_sys_mkdirs(exeDir));
    TRY(oc_sys_mkdirs(resDir));
    TRY(oc_sys_mkdirs(guestDir));
    TRY(oc_sys_mkdirs(wasmDir));
    TRY(oc_sys_mkdirs(dataDir));

    //-----------------------------------------------------------
    //NOTE: copy orca runtime executable and libraries
    //-----------------------------------------------------------
    oc_str8 orcaExe = oc_path_append(a, orcaDir, OC_STR8("bin/orca_runtime.exe"));
    oc_str8 orcaLib = oc_path_append(a, orcaDir, OC_STR8("bin/orca.dll"));
    oc_str8 glesLib = oc_path_append(a, orcaDir, OC_STR8("src/ext/angle/bin/libGLESv2.dll"));
    oc_str8 eglLib = oc_path_append(a, orcaDir, OC_STR8("src/ext/angle/bin/libEGL.dll"));

    oc_str8 exeOut = oc_path_append(a, exeDir, oc_str8_pushf(a, "%.*s.exe", oc_str8_ip(name)));
    TRY(oc_sys_copy(orcaExe, exeOut));
    TRY(oc_sys_copy(orcaLib, exeDir));
    TRY(oc_sys_copy(glesLib, exeDir));
    TRY(oc_sys_copy(eglLib, exeDir));

    //-----------------------------------------------------------
    //NOTE: copy wasm module and data
    //-----------------------------------------------------------
    TRY(oc_sys_copy(module, oc_path_append(a, wasmDir, OC_STR8("/module.wasm"))));

    oc_str8_list_for(resource_files, it)
    {
        oc_str8 resource_file = it->string;
        if(oc_sys_isdir(resource_file))
        {
            printf("Error: Got %.*s as a resource file, but it is a directory. Ignoring.", 
				oc_str8_ip(resource_file));
        }
        else
        {
            TRY(oc_sys_copy(resource_file, dataDir));
        }
    }

    oc_str8_list_for(resource_dirs, it)
    {
        oc_str8 resource_dir = it->string;
        if(oc_sys_isdir(resource_dir))
        {
            TRY(oc_sys_copytree(resource_dir, dataDir));
        }
        else
        {
            printf("Error: Got %.*s as a resource dir, but it is not a directory. Ignoring.", 
				oc_str8_ip(resource_dir));
        }
    }

    //-----------------------------------------------------------
    //NOTE: copy runtime resources
    //-----------------------------------------------------------
    TRY(oc_sys_copy(oc_path_append(a, orcaDir, OC_STR8("resources/Menlo.ttf")), resDir));
    TRY(oc_sys_copy(oc_path_append(a, orcaDir, OC_STR8("resources/Menlo Bold.ttf")), resDir));

    //-----------------------------------------------------------
    //NOTE make icon
    //-----------------------------------------------------------
    //TODO

    return 0;
}
#elif OC_PLATFORM_MACOS

int macBundle(
    oc_arena* a,
    oc_str8 name,
    oc_str8 icon,
    oc_str8 version,
    oc_str8_list resource_files,
    oc_str8_list resource_dirs,
    oc_str8 outDir,
    oc_str8 module,
	bool mtlEnableCapture)
{
    //-----------------------------------------------------------
    //NOTE: make bundle directory structure
    //-----------------------------------------------------------
    oc_str8_list list = { 0 };
    oc_str8_list_push(a, &list, name);
    oc_str8_list_push(a, &list, OC_STR8(".app"));
    name = oc_str8_list_join(a, list);

	oc_str8 orcaDir = get_current_version_dir(a);
    oc_str8 bundleDir = oc_path_append(a, outDir, name);
    oc_str8 contentsDir = oc_path_append(a, bundleDir, OC_STR8("Contents"));
    oc_str8 exeDir = oc_path_append(a, contentsDir, OC_STR8("MacOS"));
    oc_str8 resDir = oc_path_append(a, contentsDir, OC_STR8("resources"));
    oc_str8 guestDir = oc_path_append(a, contentsDir, OC_STR8("app"));
    oc_str8 wasmDir = oc_path_append(a, guestDir, OC_STR8("wasm"));
    oc_str8 dataDir = oc_path_append(a, guestDir, OC_STR8("data"));

    if(oc_sys_exists(bundleDir))
    {
        TRY(oc_sys_rmdir(bundleDir));
    }
    TRY(oc_sys_mkdirs(bundleDir));
    TRY(oc_sys_mkdirs(contentsDir));
    TRY(oc_sys_mkdirs(exeDir));
    TRY(oc_sys_mkdirs(resDir));
    TRY(oc_sys_mkdirs(guestDir));
    TRY(oc_sys_mkdirs(wasmDir));
    TRY(oc_sys_mkdirs(dataDir));

    //-----------------------------------------------------------
    //NOTE: copy orca runtime executable and libraries
    //-----------------------------------------------------------
    oc_str8 orcaExe = oc_path_append(a, orcaDir, OC_STR8("bin/orca_runtime"));
    oc_str8 orcaLib = oc_path_append(a, orcaDir, OC_STR8("bin/liborca.dylib"));
    oc_str8 glesLib = oc_path_append(a, orcaDir, OC_STR8("src/ext/angle/bin/libGLESv2.dylib"));
    oc_str8 eglLib = oc_path_append(a, orcaDir, OC_STR8("src/ext/angle/bin/libEGL.dylib"));
    oc_str8 renderer_lib = oc_path_append(a, orcaDir, OC_STR8("bin/mtl_renderer.metallib"));

    TRY(oc_sys_copy(orcaExe, exeDir));
    TRY(oc_sys_copy(orcaLib, exeDir));
    TRY(oc_sys_copy(glesLib, exeDir));
    TRY(oc_sys_copy(eglLib, exeDir));
    TRY(oc_sys_copy(renderer_lib, exeDir));

    //-----------------------------------------------------------
    //NOTE: copy wasm module and data
    //-----------------------------------------------------------
    TRY(oc_sys_copy(module, oc_path_append(a, wasmDir, OC_STR8("/module.wasm"))));

    oc_str8_list_for(resource_files, it)
    {
        oc_str8 resource_file = it->string;
        if(oc_sys_isdir(resource_file))
        {
            printf("Error: Got %.*s as a resource file, but it is a directory. Ignoring.",
				oc_str8_ip(resource_file));
        }
        else
        {
            TRY(oc_sys_copy(resource_file, dataDir));
        }
    }

    oc_str8_list_for(resource_dirs, it)
    {
        oc_str8 resource_dir = it->string;
        if(oc_sys_isdir(resource_dir))
        {
			// NOTE(shaw): trailing slash means that contents are copied rather
			// than the directory itself
			oc_str8 resource_dir_slash = oc_path_append(a, resource_dir, OC_STR8("/"));
			TRY(oc_sys_copytree(resource_dir_slash, dataDir));
        }
        else
        {
            printf("Error: Got %.*s as a resource dir, but it is not a directory. Ignoring.", 
				oc_str8_ip(resource_dir));
        }
    }

    //-----------------------------------------------------------
    //NOTE: copy runtime resources
    //-----------------------------------------------------------
    TRY(oc_sys_copy(oc_path_append(a, orcaDir, OC_STR8("resources/Menlo.ttf")), resDir));
    TRY(oc_sys_copy(oc_path_append(a, orcaDir, OC_STR8("resources/Menlo Bold.ttf")), resDir));

    //-----------------------------------------------------------
    //NOTE make icon
    //-----------------------------------------------------------
    //TODO

	//-----------------------------------------------------------
	//NOTE: write plist file
	//-----------------------------------------------------------
	oc_str8 bundle_sig = OC_STR8("????");
	oc_str8 icon_file = OC_STR8("");

	oc_str8 plist_contents = oc_str8_pushf(a,
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
"<plist version=\"1.0\">"
	"<dict>"
		"<key>CFBundleName</key>"
		"<string>%.*s</string>"
		"<key>CFBundleDisplayName</key>"
		"<string>%.*s</string>"
		"<key>CFBundleIdentifier</key>"
		"<string>%.*s</string>"
		"<key>CFBundleVersion</key>"
		"<string>%.*s</string>"
		"<key>CFBundlePackageType</key>"
		"<string>APPL</string>"
		"<key>CFBundleSignature</key>"
		"<string>%.*s</string>"
		"<key>CFBundleExecutable</key>"
		"<string>orca_runtime</string>"
		"<key>CFBundleIconFile</key>"
		"<string>icon.icns</string>"
		"<key>NSHighResolutionCapable</key>"
		"<string>True</string>"
		"%s"
	"</dict>"
"</plist>",
		oc_str8_printf(name),
		oc_str8_printf(name),
		oc_str8_printf(name),
		oc_str8_printf(version),
		oc_str8_printf(bundle_sig),
		mtlEnableCapture ? "<key>MetalCaptureEnabled</key><true/>" : "");

	oc_str8 plist_path = oc_path_append(a, contentsDir, OC_STR8("Info.plist"));
	oc_file plist_file = oc_file_open(plist_path, OC_FILE_ACCESS_WRITE, OC_FILE_OPEN_CREATE);
	if(oc_file_is_nil(plist_file)) 
	{
		fprintf(stderr, "Error: failed to create plist file \"%.*s\"\n", 
			oc_str8_printf(plist_path));
		oc_file_close(plist_file);
		return 1;
	} 
	oc_file_write(plist_file, plist_contents.len, plist_contents.ptr);
	oc_file_close(plist_file);

    return 0;
}

#endif
