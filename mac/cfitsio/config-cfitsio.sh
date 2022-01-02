#!/bin/bash
cd ../../external/cfitsio
export CC=cc
export CFLAGS="-arch x86_64 -arch arm64 -g -O2 -mmacosx-version-min=10.13"
./configure --prefix=$PWD  --disable-curl
