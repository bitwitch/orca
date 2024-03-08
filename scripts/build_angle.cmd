@echo off
setlocal enabledelayedexpansion

set PATH=%CD%\depot_tools;%PATH%

rem *** check dependencies ***

where /q python.exe || (
    echo ERROR: "python.exe" not found
    exit /b 1
)

where /q git.exe || (
    echo ERROR: "git.exe" not found
    exit /b 1
)

where /q curl.exe || (
    echo ERROR: "curl.exe" not found
    exit /b 1
)

if exist "%ProgramFiles%\7-Zip\7z.exe" (
  set SZIP="%ProgramFiles%\7-Zip\7z.exe"
) else (
  where /q 7za.exe || (
    echo ERROR: 7-Zip installation or "7za.exe" not found
    exit /b 1
  )
  set SZIP=7za.exe
)

if "%GITHUB_ACTIONS%" neq "" (
    where /q tar.exe || (
      echo ERROR: "tar.exe" not found
      exit /b 1
    )
)

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -version [17.0^,18.0^) -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath`) do set VS=%%i
if "!VS!" equ "" (
    echo ERROR: Visual Studio 2022 installation not found
    exit /b 1
)  

rem *** download depot_tools ***

if not exist depot_tools (
    mkdir depot_tools
    pushd depot_tools
    curl -LOsf https://storage.googleapis.com/chrome-infra/depot_tools.zip || exit /b 1
    %SZIP% x -bb0 -y depot_tools.zip 1>nul 2>nul || exit /b 1
    del depot_tools.zip 1>nul 2>nul
    popd
)

rem *** download angle source ***

if exist angle_src (
    pushd angle_src
    pushd build
    call git reset --hard HEAD
    popd
    call git pull --force --no-tags --depth 1
    popd
) else (
    call git clone --single-branch --no-tags --depth 1 https://chromium.googlesource.com/angle/angle angle_src || exit /b 1
    pushd angle_src
    python scripts\bootstrap.py || exit /b 1
    popd
)

rem *** build angle ***

pushd angle_src

set DEPOT_TOOLS_WIN_TOOLCHAIN=0
call gclient sync || exit /b 1
call gn gen out/Release --args="angle_build_all=false is_debug=false is_component_build=false angle_has_frame_capture=false angle_enable_gl=false angle_enable_vulkan=false angle_enable_d3d9=false angle_enable_null=false" || exit /b 1
call autoninja -C out/Release libEGL libGLESv2 libGLESv1_CM || exit /b 1
popd

rem *** prepare output folder ***

mkdir angle
mkdir angle\bin
mkdir angle\lib
mkdir angle\include

copy /y angle_src\.git\refs\heads\main angle\commit.txt 1>nul 2>nul

copy /y "%ProgramFiles(x86)%\Windows Kits\10\Redist\D3D\x64\d3dcompiler_47.dll" angle\bin 1>nul 2>nul

copy /y angle_src\out\Release\libEGL.dll       angle\bin        1>nul 2>nul
copy /y angle_src\out\Release\libGLESv1_CM.dll angle\bin        1>nul 2>nul
copy /y angle_src\out\Release\libGLESv2.dll    angle\bin        1>nul 2>nul

copy /y angle_src\out\Release\libEGL.dll.lib       angle\lib    1>nul 2>nul
copy /y angle_src\out\Release\libGLESv1_CM.dll.lib angle\lib    1>nul 2>nul
copy /y angle_src\out\Release\libGLESv2.dll.lib    angle\lib    1>nul 2>nul

xcopy /D /S /I /Q /Y angle_src\include\KHR   angle\include\KHR   1>nul 2>nul
xcopy /D /S /I /Q /Y angle_src\include\EGL   angle\include\EGL   1>nul 2>nul
xcopy /D /S /I /Q /Y angle_src\include\GLES  angle\include\GLES  1>nul 2>nul
xcopy /D /S /I /Q /Y angle_src\include\GLES2 angle\include\GLES2 1>nul 2>nul
xcopy /D /S /I /Q /Y angle_src\include\GLES3 angle\include\GLES3 1>nul 2>nul

del /Q /S angle\include\*.clang-format 1>nul 2>nul

rem *** done ***
rem output is in angle folder

if "%GITHUB_ACTIONS%" neq "" (
    set /p ANGLE_COMMIT=<angle\commit.txt

    for /F "skip=1" %%D in ('WMIC OS GET LocalDateTime') do (set LDATE=%%D & goto :dateok)
    :dateok
    set BUILD_DATE=%LDATE:~0,4%-%LDATE:~4,2%-%LDATE:~6,2%

    tar -czf angle-%BUILD_OS%-%GITHUB_REF_NAME%.tar.gz angle || exit /b 1

    echo ANGLE_COMMIT=%ANGLE_COMMIT%>>%GITHUB_OUTPUT%
    echo BUILD_DATE=%BUILD_DATE%>>%GITHUB_OUTPUT%
    echo GITHUB_REF_NAME=%GITHUB_REF_NAME%>>%GITHUB_OUTPUT%
)
