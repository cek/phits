#!/bin/sh
# An script for use with xcode to build cfitsio such that static libraries are copied
# to a separate staticlib dir. We do this so that we can easily link against the static
# libs, rather than the dynamic libs.
#
# The script assumes that the project has been configured such that the lib
# directory is in the source directory.
cd ../../external/cfitsio
if [ "$1" == "clean" ]
then
    make clean
    /bin/rm -rf staticlib
else
    make -j12 $1
    if [ ! -e staticlib/pkgconfig ]; then mkdir -p staticlib/pkgconfig; fi
    /bin/cp lib/*.a staticlib
    sed 's/\/lib/\/staticlib/' < lib/pkgconfig/cfitsio.pc > staticlib/pkgconfig/cfitsio.pc
fi
