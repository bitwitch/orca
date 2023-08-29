import glob
import os
import platform
import urllib.request
import shutil
import subprocess
from zipfile import ZipFile

from . import checksum
from .bindgen import bindgen
from .gles_gen import gles_gen
from .log import *
from .utils import pushd, removeall, yeetdir, yeetfile
from .embed_text_files import *
from .version import check_if_source, is_orca_source, orca_version

ANGLE_VERSION = "2023-07-05"


def attach_dev_commands(subparsers):
    dev_cmd = subparsers.add_parser("dev", help="Commands for building Orca itself. Must be run from within an Orca source checkout.")
    dev_cmd.set_defaults(func=orca_source_only)

    dev_sub = dev_cmd.add_subparsers(required=is_orca_source(), title='commands')

    build_cmd = dev_sub.add_parser("build-runtime", help="Build the Orca runtime from source.")
    build_cmd.add_argument("--release", action="store_true", help="compile Orca in release mode (default is debug)")
    build_cmd.set_defaults(func=dev_shellish(build_runtime))

    clean_cmd = dev_sub.add_parser("clean", help="Delete all build artifacts and start fresh.")
    clean_cmd.set_defaults(func=dev_shellish(clean))

    install_cmd = dev_sub.add_parser("install", help="Install the Orca tools into a system folder.")
    install_cmd.add_argument("--no-confirm", action="store_true", help="don't ask the user for confirmation before installing")
    install_cmd.set_defaults(func=dev_shellish(install))

    uninstall_cmd = dev_sub.add_parser("uninstall", help="Uninstall the system installation of Orca.")
    uninstall_cmd.set_defaults(func=dev_shellish(uninstall))


def orca_source_only(args):
    print("The Orca dev commands can only be run from an Orca source checkout.")
    print()
    print("If you want to build Orca yourself, download the source here:")
    print("https://git.handmade.network/hmn/orca")
    exit(1)


def dev_shellish(func):
    use_source, source_dir, _ = check_if_source()
    if not use_source:
        return orca_source_only

    def func_from_source(args):
        os.chdir(source_dir)
        func(args)
    return shellish(func_from_source)


def build_runtime(args):
    ensure_programs()
    ensure_angle()

    build_platform_layer("lib", args.release)
    build_wasm3(args.release)
    build_orca(args.release)


def clean(args):
    yeetdir("build")
    yeetdir("src/ext/angle")
    yeetdir("scripts/files")
    yeetdir("scripts/__pycache__")


def build_platform_layer(target, release):
    print("Building Orca platform layer...")

    os.makedirs("build/bin", exist_ok=True)
    os.makedirs("build/lib", exist_ok=True)

    if target == "lib":
        if platform.system() == "Windows":
            build_platform_layer_lib_win(release)
        elif platform.system() == "Darwin":
            build_platform_layer_lib_mac(release)
        else:
            log_error(f"can't build platform layer for unknown platform '{platform.system()}'")
            exit(1)
    elif target == "test":
        with pushd("examples/test_app"):
            # TODO?
            subprocess.run(["./build.sh"])
    elif target == "clean":
        removeall("bin")
    else:
        log_error(f"unrecognized platform layer target '{target}'")
        exit(1)


def build_platform_layer_lib_win(release):
    embed_text_files("src\\graphics\\glsl_shaders.h", "glsl_", [
        "src\\graphics\\glsl_shaders\\common.glsl",
        "src\\graphics\\glsl_shaders\\blit_vertex.glsl",
        "src\\graphics\\glsl_shaders\\blit_fragment.glsl",
        "src\\graphics\\glsl_shaders\\path_setup.glsl",
        "src\\graphics\\glsl_shaders\\segment_setup.glsl",
        "src\\graphics\\glsl_shaders\\backprop.glsl",
        "src\\graphics\\glsl_shaders\\merge.glsl",
        "src\\graphics\\glsl_shaders\\raster.glsl",
        "src\\graphics\\glsl_shaders\\balance_workgroups.glsl",
    ])

    includes = [
        "/I", "src",
        "/I", "src/ext",
        "/I", "src/ext/angle/include",
    ]
    libs = [
        "user32.lib",
        "opengl32.lib",
        "gdi32.lib",
        "shcore.lib",
        "delayimp.lib",
        "dwmapi.lib",
        "comctl32.lib",
        "ole32.lib",
        "shell32.lib",
        "shlwapi.lib",
        "dxgi.lib",
        "dxguid.lib",
        "/LIBPATH:src/ext/angle/lib",
        "libEGL.dll.lib",
        "libGLESv2.dll.lib",
        "/DELAYLOAD:libEGL.dll",
        "/DELAYLOAD:libGLESv2.dll",
    ]

    subprocess.run([
        "cl", "/nologo",
        "/we4013", "/Zi", "/Zc:preprocessor",
        "/DOC_BUILD_DLL",
        "/std:c11", "/experimental:c11atomics",
        *includes,
        "src/orca.c", "/Fo:build/bin/orca.o",
        "/LD", "/link",
        "/MANIFEST:EMBED", "/MANIFESTINPUT:src/app/win32_manifest.xml",
        *libs,
        "/OUT:build/bin/orca.dll",
        "/IMPLIB:build/bin/orca.dll.lib",
    ], check=True)

def build_platform_layer_lib_mac(release):
    sdk_dir = "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"

    flags = ["-mmacos-version-min=10.15.4", "-maes"]
    cflags = ["-std=c11"]
    debug_flags = ["-O3"] if release else ["-g", "-DOC_DEBUG", "-DOC_LOG_COMPILE_DEBUG"]
    ldflags = [f"-L{sdk_dir}/usr/lib", f"-F{sdk_dir}/System/Library/Frameworks/"]
    includes = ["-Isrc", "-Isrc/util", "-Isrc/platform", "-Isrc/ext", "-Isrc/ext/angle/include"]

    # compile metal shader
    subprocess.run([
        "xcrun", "-sdk", "macosx", "metal",
        # TODO: shaderFlagParam
        "-fno-fast-math", "-c",
        "-o", "build/mtl_renderer.air",
        "src/graphics/mtl_renderer.metal",
    ], check=True)
    subprocess.run([
        "xcrun", "-sdk", "macosx", "metallib",
        "-o", "build/bin/mtl_renderer.metallib",
        "build/mtl_renderer.air",
    ], check=True)

    # compile platform layer. We use one compilation unit for all C code, and one
    # compilation unit for all Objective-C code
    subprocess.run([
        "clang",
        *debug_flags, "-c",
        "-o", "build/orca_c.o",
        *cflags, *flags, *includes,
        "src/orca.c"
    ], check=True)
    subprocess.run([
        "clang",
        *debug_flags, "-c",
        "-o", "build/orca_objc.o",
        *flags, *includes,
        "src/orca.m"
    ], check=True)

    # build dynamic library
    subprocess.run([
        "ld",
        *ldflags, "-dylib",
        "-o", "build/bin/liborca.dylib",
        "build/orca_c.o", "build/orca_objc.o",
        "-Lsrc/ext/angle/bin", "-lc",
        "-framework", "Carbon", "-framework", "Cocoa", "-framework", "Metal", "-framework", "QuartzCore",
        "-weak-lEGL", "-weak-lGLESv2",
    ], check=True)

    # change dependent libs path to @rpath
    subprocess.run([
        "install_name_tool",
        "-change", "./libEGL.dylib", "@rpath/libEGL.dylib",
        "build/bin/liborca.dylib",
    ], check=True)
    subprocess.run([
        "install_name_tool",
        "-change", "./libGLESv2.dylib", "@rpath/libGLESv2.dylib",
        "build/bin/liborca.dylib",
    ], check=True)

    # add executable path to rpath. Client executable can still add its own
    # rpaths if needed, e.g. @executable_path/libs/ etc.
    subprocess.run([
        "install_name_tool",
        "-id", "@rpath/liborca.dylib",
        "build/bin/liborca.dylib",
    ], check=True)


def build_wasm3(release):
    print("Building wasm3...")

    os.makedirs("build/bin", exist_ok=True)
    os.makedirs("build/lib", exist_ok=True)
    os.makedirs("build/obj", exist_ok=True)

    if platform.system() == "Windows":
        build_wasm3_lib_win(release)
    elif platform.system() == "Darwin":
        build_wasm3_lib_mac(release)
    else:
        log_error(f"can't build wasm3 for unknown platform '{platform.system()}'")
        exit(1)


def build_wasm3_lib_win(release):
    for f in glob.iglob("./src/ext/wasm3/source/*.c"):
        name = os.path.splitext(os.path.basename(f))[0]
        subprocess.run([
            "cl", "/nologo",
            "/Zi", "/Zc:preprocessor", "/c",
            "/O2",
            f"/Fo:build/obj/{name}.obj",
            "/I", "./src/ext/wasm3/source",
            f,
        ], check=True)
    subprocess.run([
        "lib", "/nologo", "/out:build/bin/wasm3.lib",
        "build/obj/*.obj",
    ], check=True)


def build_wasm3_lib_mac(release):
    includes = ["-Isrc/ext/wasm3/source"]
    debug_flags = ["-g", "-O2"]
    flags = [
        *debug_flags,
        "-foptimize-sibling-calls",
        "-Wno-extern-initializer",
        "-Dd_m3VerboseErrorMessages",
    ]

    for f in glob.iglob("src/ext/wasm3/source/*.c"):
        name = os.path.splitext(os.path.basename(f))[0] + ".o"
        subprocess.run([
            "clang", "-c", *flags, *includes,
            "-o", f"build/obj/{name}",
            f,
        ], check=True)
    subprocess.run(["ar", "-rcs", "build/lib/libwasm3.a", *glob.glob("build/obj/*.o")], check=True)
    subprocess.run(["rm", "-rf", "build/obj"], check=True)


def build_orca(release):
    print("Building Orca runtime...")

    os.makedirs("build/bin", exist_ok=True)
    os.makedirs("build/lib", exist_ok=True)

    if platform.system() == "Windows":
        build_orca_win(release)
    elif platform.system() == "Darwin":
        build_orca_mac(release)
    else:
        log_error(f"can't build Orca for unknown platform '{platform.system()}'")
        exit(1)


def build_orca_win(release):

    gen_all_bindings()

    # compile orca
    includes = [
        "/I", "src",
        "/I", "src/ext",
        "/I", "src/ext/angle/include",
        "/I", "src/ext/wasm3/source",
    ]
    libs = [
        "/LIBPATH:build/bin",
        "orca.dll.lib",
        "wasm3.lib",
    ]

    subprocess.run([
        "cl",
        "/Zi", "/Zc:preprocessor",
        "/std:c11", "/experimental:c11atomics",
        *includes,
        "src/runtime.c",
        "/link", *libs,
        "/out:build/bin/orca_runtime.exe",
    ], check=True)


def build_orca_mac(release):

    includes = [
        "-Isrc",
        "-Isrc/ext",
        "-Isrc/ext/angle/include",
        "-Isrc/ext/wasm3/source"
    ]
    libs = ["-Lbuild/bin", "-Lbuild/lib", "-lorca", "-lwasm3"]
    debug_flags = ["-O2"] if release else ["-g", "-DOC_DEBUG -DOC_LOG_COMPILE_DEBUG"]
    flags = [
        *debug_flags,
        "-mmacos-version-min=10.15.4",
        "-maes",
    ]

    gen_all_bindings()

    # compile orca
    subprocess.run([
        "clang", *flags, *includes, *libs,
        "-o", "build/bin/orca_runtime",
        "src/runtime.c",
    ], check=True)

    # fix libs imports
    subprocess.run([
        "install_name_tool",
        "-change", "build/bin/liborca.dylib", "@rpath/liborca.dylib",
        "build/bin/orca_runtime",
    ], check=True)
    subprocess.run([
        "install_name_tool",
        "-add_rpath", "@executable_path/",
        "build/bin/orca_runtime",
    ], check=True)


def gen_all_bindings():
    gles_gen("src/ext/gl.xml",
        "src/wasmbind/gles_api.json",
        "src/graphics/orca_gl31.h"
    )

    bindgen("gles", "src/wasmbind/gles_api.json",
        wasm3_bindings="src/wasmbind/gles_api_bind_gen.c",
    )

    bindgen("core", "src/wasmbind/core_api.json",
        guest_stubs="src/wasmbind/core_api_stubs.c",
        wasm3_bindings="src/wasmbind/core_api_bind_gen.c",
    )

    bindgen("surface", "src/wasmbind/surface_api.json",
        guest_stubs="src/graphics/orca_surface_stubs.c",
        guest_include="graphics/graphics.h",
        wasm3_bindings="src/wasmbind/surface_api_bind_gen.c",
    )

    bindgen("clock", "src/wasmbind/clock_api.json",
        guest_include="platform/platform_clock.h",
        wasm3_bindings="src/wasmbind/clock_api_bind_gen.c",
    )
    bindgen("io", "src/wasmbind/io_api.json",
        guest_stubs="src/platform/orca_io_stubs.c",
        wasm3_bindings="src/wasmbind/io_api_bind_gen.c",
    )


def ensure_programs():
    if platform.system() == "Windows":
        try:
            subprocess.run(["cl"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except FileNotFoundError:
            msg = log_error("MSVC was not found on your system.")
            msg.more("If you have already installed Visual Studio, make sure you are running in a")
            msg.more("Visual Studio command prompt or you have run vcvarsall.bat. Otherwise, download")
            msg.more("and install Visual Studio: https://visualstudio.microsoft.com/")
            exit(1)

    try:
        subprocess.run(["clang", "-v"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except FileNotFoundError:
        msg = log_error("clang was not found on your system.")
        if platform.system() == "Windows":
            msg.more("We recommend installing clang via the Visual Studio installer.")
        elif platform.system() == "Darwin":
            msg.more("Run the following to install it:")
            msg.more()
            msg.more("  brew install llvm")
            msg.more()
        exit(1)
    # TODO(ben): Check for xcode command line tools


def ensure_angle():
    if not verify_angle():
        download_angle()
        print("Verifying ANGLE download...")
        if not verify_angle():
            log_error("automatic ANGLE download failed")
            exit(1)


def verify_angle():
    checkfiles = None
    if platform.system() == "Windows":
        checkfiles = [
            "src/ext/angle/bin/libEGL.dll",
            "src/ext/angle/lib/libEGL.dll.lib",
            "src/ext/angle/bin/libGLESv2.dll",
            "src/ext/angle/lib/libGLESv2.dll.lib",
        ]
    elif platform.system() == "Darwin":
        checkfiles = [
            "src/ext/angle/bin/libEGL.dylib",
            "src/ext/angle/bin/libGLESv2.dylib",
        ]

    if checkfiles is None:
        log_warning("could not verify if the correct version of ANGLE is present")
        return False

    ok = True
    for file in checkfiles:
        if not os.path.isfile(file):
            ok = False
            continue
        if not checksum.checkfile(file):
            ok = False
            continue

    return ok


def download_angle():
    print("Downloading ANGLE...")
    if platform.system() == "Windows":
        build = "windows-2019"
    elif platform.system() == "Darwin":
        build = "macos-jank"
    else:
        log_error(f"could not automatically download ANGLE for unknown platform {platform.system()}")
        return

    os.makedirs("scripts/files", exist_ok=True)
    filename = f"angle-{build}-{ANGLE_VERSION}.zip"
    filepath = f"scripts/files/{filename}"
    url = f"https://github.com/HandmadeNetwork/build-angle/releases/download/{ANGLE_VERSION}/{filename}"
    with urllib.request.urlopen(url) as response:
        with open(filepath, "wb") as out:
            shutil.copyfileobj(response, out)

    if not checksum.checkfile(filepath):
        log_error(f"ANGLE download did not match checksum")
        exit(1)

    print("Extracting ANGLE...")
    with ZipFile(filepath, "r") as anglezip:
        anglezip.extractall(path="scripts/files")

    shutil.copytree(f"scripts/files/angle/", "src/ext/angle", dirs_exist_ok=True)


def prompt(msg):
    while True:
        answer = input(f"{msg} (y/n)> ")
        if answer.lower() in ["y", "yes"]:
            return True
        elif answer.lower() in ["n", "no"]:
            return False
        else:
            print("Please enter \"yes\" or \"no\" and press return.")

def install_dir():
    if platform.system() == "Windows":
        return os.path.join(os.getenv("LOCALAPPDATA"), "orca")
    else:
        return os.path.expanduser(os.path.join("~", ".orca"))


def install(args):
    dest = install_dir()
    bin_dir = os.path.join(dest, "bin")
    src_dir = os.path.join(dest, "src")
    version_file = os.path.join(dest, ".orcaversion")

    version = orca_version()
    existing_version = None
    try:
        with open(version_file, "r") as f:
            existing_version = f.read().strip()
    except FileNotFoundError:
        pass

    if not args.no_confirm:
        print(f"The Orca command-line tools (version {version}) will be installed to:")
        print(dest)
        print()
        if existing_version is not None:
            print(f"This will overwrite version {existing_version}.")
            print()
        if not prompt("Proceed with the installation?"):
            return

    yeetdir(bin_dir)
    yeetdir(src_dir)
    yeetfile(version_file)

    # The MS Store version of Python does some really stupid stuff with AppData:
    # https://git.handmade.network/hmn/orca/issues/32
    #
    # Any new files and folders created in AppData actually get created in a special
    # folder specific to the Python version. However, if the files or folders already
    # exist, the redirect does not happen. So, if we first use the shell to create the
    # paths we need, the following scripts work regardless of Python install.
    #
    # Also apparently you can't just do mkdir in a subprocess call here, hence the
    # trivial batch scripts.
    if platform.system() == "Windows":
        subprocess.run(["scripts\\mkdir.bat", bin_dir], check=True)
        subprocess.run(["scripts\\mkdir.bat", src_dir], check=True)
        subprocess.run(["scripts\\touch.bat", version_file], check=True)

    shutil.copytree("scripts", os.path.join(bin_dir, "sys_scripts"))
    shutil.copy("orca", bin_dir)
    shutil.copytree("src", src_dir, dirs_exist_ok=True)
    if platform.system() == "Windows":
        shutil.copy("orca.bat", bin_dir)
    with open(version_file, "w") as f:
        f.write(version)

    print()
    if platform.system() == "Windows":
        print("The Orca tools have been installed to the following directory:")
        print(bin_dir)
        print()
        print("The tools will need to be on your PATH in order to actually use them.")
        if prompt("Would you like to automatically add Orca to your PATH?"):
            subprocess.run(["powershell", "scripts\\updatepath.ps1", bin_dir], check=True)
            print("Orca has been added to your PATH. Restart any open terminals to use it.")
        else:
            print("No worries. You can manually add Orca to your PATH in the Windows settings")
            print("by searching for \"environment variables\".")
    else:
        print("The Orca tools have been installed. Make sure the Orca tools are on your PATH by")
        print("adding the following to your shell config:")
        print()
        print(f"export PATH=\"{bin_dir}:$PATH\"")
    print()


def uninstall(args):
    orca_dir, bin_dir = install_path()

    if not os.path.exists(orca_dir):
        print("Orca is not installed on your system.")
        exit()

    print(f"Orca is currently installed at {orca_dir}.")
    if prompt("Are you sure you want to uninstall?"):
        yeet(orca_dir)

        if platform.system() == "Windows":
            print("Orca has been uninstalled from your system.")
            print()
            if prompt("Would you like to automatically remove Orca from your PATH?"):
                subprocess.run(["powershell", "scripts\\updatepath.ps1", bin_dir, "-remove"], check=True)
                print("Orca has been removed from your PATH.")
        else:
            print("Orca has been uninstalled from your system. You may wish to remove it from your PATH.")
