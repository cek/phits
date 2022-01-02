#!/bin/sh
cd ../../external/CCfits
export CXX=c++
# Why does running the plugin fail if -O2 is not specified?
export CXXFLAGS="-g -O2 -arch x86_64 -arch arm64 -mmacosx-version-min=10.13"
./configure --enable-static=true --enable-shared=false --with-cfitsio-include=$PWD/../cfitsio/include --with-cfitsio-libdir=$PWD/../cfitsio/staticlib --prefix=$PWD
