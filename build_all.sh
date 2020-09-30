#!/bin/bash

##### linux x86-64 gcc
build=build-linux-x86_64-gcc
rm -rf $build && mkdir -p $build 
pushd $build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/x86_64-linux.gcc.toolchain.cmake
make
popd

###### linux x86-64 clang
#build=build-linux-x86_64-clang
#rm -rf $build && mkdir -p $build
#pushd $build
#cmake .. \
#    -DCMAKE_TOOLCHAIN_FILE=../cmake/x86_64-linux.clang.toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
#make
#popd

###### android armv8
#build=build-android-armv8
#rm -rf $build && mkdir -p $build 
#pushd $build
#cmake .. \
#    -DCMAKE_TOOLCHAIN_FILE=$NDK_HOME/build/cmake/android.toolchain.cmake \
#    -DANDROID_STL=c++_static \
#    -DANDROID_ABI="arm64-v8a" \
#    -DANDROID_PLATFORM=android-27
#make
#popd

