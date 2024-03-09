#!/bin/bash

set -euxo pipefail

export PATH="$(pwd)/depot_tools:$PATH"

# Download depot_tools
if [ ! -d "depot_tools" ]; then
    mkdir depot_tools
    pushd depot_tools
    curl -L -O https://storage.googleapis.com/chrome-infra/depot_tools.zip
    unzip depot_tools.zip
    rm depot_tools.zip
    popd
fi

# Download ANGLE source
if [ -d "angle_src" ]; then
    pushd angle_src
    if [ -d "build" ]; then
        pushd build
        git reset --hard HEAD
        popd
    fi
    git pull --force --no-tags --depth 1
    popd
else
    git clone --single-branch --no-tags --depth 1 https://chromium.googlesource.com/angle/angle angle_src
    pushd angle_src
    python3 scripts/bootstrap.py
    popd
fi

# Build ANGLE
pushd angle_src
gclient sync
gn gen out/Release --args="is_debug=false is_component_build=false angle_enable_metal=true angle_enable_glsl=true angle_enable_d3d9=false angle_enable_d3d11=false angle_enable_gl=false angle_enable_null=false angle_enable_vulkan=false angle_enable_essl=false"
autoninja -C out/Release
popd

# Prepare output folder
mkdir -p angle/bin
mkdir -p angle/include

cp angle_src/.git/refs/heads/main angle/commit.txt

cp angle_src/out/Release/libEGL.dylib angle/bin
cp angle_src/out/Release/libGLESv1_CM.dylib angle/bin
cp angle_src/out/Release/libGLESv2.dylib angle/bin

cp -R angle_src/include/KHR angle/include/KHR
cp -R angle_src/include/EGL angle/include/EGL
cp -R angle_src/include/GLES angle/include/GLES
cp -R angle_src/include/GLES2 angle/include/GLES2
cp -R angle_src/include/GLES3 angle/include/GLES3

# Zip up ANGLE folder
if [ ! -z "${GITHUB_ACTIONS}" ]; then
    ANGLE_COMMIT=$(cat angle/commit.txt)
    BUILD_DATE=$(date +%Y-%m-%d)

    tar -czf angle-$BUILD_OS-$GITHUB_REF_NAME.tar.gz angle

    echo "ANGLE_COMMIT=$ANGLE_COMMIT" >> $GITHUB_OUTPUT
    echo "BUILD_DATE=$BUILD_DATE" >> $GITHUB_OUTPUT
    echo "GITHUB_REF_NAME=$GITHUB_REF_NAME" >> $GITHUB_OUTPUT
fi
