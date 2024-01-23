@echo off

set ORCA_DIR=../..

:: common flags to build wasm modules
set wasmFlags=--target=wasm32^
       -mbulk-memory ^
       -g -O2 ^
       -D__ORCA__ ^
       -Wl,--no-entry ^
       -Wl,--export-dynamic ^
       --sysroot %ORCA_DIR%/build/orca-libc ^
       -I%ORCA_DIR%/src ^
       -I%ORCA_DIR%/src/ext

clang %wasmFlags% -L %ORCA_DIR%/build/bin -lorca_wasm -o files.wasm files.c
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

call orca bundle --orca-dir %ORCA_DIR% --name Tests --resource-dir data files.wasm
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

Tests\bin\Tests.exe --test=files.wasm
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%
