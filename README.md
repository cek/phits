# Phits Photoshop plug-in for FITS images

![M31](images/m31.png)

Phits is a plug-in for reading and writing [FITS](https://en.wikipedia.org/wiki/FITS) images using recent versions of Photoshop.
It is geared towards astrophotography post-processing, rather than general-purpose FITS file manipulation.

## Features ##

Phits is capable of reading 8-, 16-, 32-, and 64-bit integer and 32- and 64-bit floating-point FITS images, with either 1, 3, or 4
channels. It supports saving FITS files in 8- or 16-bit per channel integer, or 32-bit per channel floating-point formats.

Most FITS images will be converted to normalized 32-bit floating point values when read by Phits. This is due to the fact that
Photoshop only operates on 8- or 15-bit integer data, or floating point data in the range [0,1]. In particular, only 8-bit data with
FITS metadata values bzero=0 and bscale=1, or floating-point images with values in the range [0,1] will be imported without the
data being transformed.

In this alpha release, normalization is always performed by mapping the minimum image data value to zero, and the maximum value to one.
A forthcoming release will provide the ability to choose from several normalization methods.

## Limitations ##

Phits is only capable of reading and writing the FITS Primary HDU. As a result, there is currently no way to read or write anything
other than the primary FITS image. If you attempt to save to a FITS file that contained extension (i.e., additional) data when read,
Phits will issue a warning to help prevent accidental data loss.

Primary HDU keywords and associated medata are preserved when saving a FITS file. However, keyword order is currently not preserved.

## Compatibility ##

This alpha release has beeen tested on Windows 10 and macOS 11 and 12 (Intel and Apple silicon), using Photoshop 2021 and 2022.
Earlier operating system or Photoshop versions will probably not work.

## Building ##

### Prerequisites ###

Phits is built using versions of
[cfitsio]( https://heasarc.gsfc.nasa.gov/fitsio/)
and
[CCfits](https://heasarc.gsfc.nasa.gov/fitsio/CCfits)
that have slightly modified to support opening files outside of the FITS libraries. These modified versions are provided as
submodules. Alternatively, source to the modified versions of [cfitsio](https://github.com/cek/cfitsio) and
[CCfits](http://github.com/cek/CCfits) can be downloaded separately.

You will also need a copy of the [Adobe Photoshop API](https://www.adobe.io/photoshop/api/), which must be downloaded separately
and extracted to `external/photoshopsdk` due to licensing restrictions.

If compiling on Windows, you will also need [CMake](https://cmake.org).

### macOS ###

First, run the initial configuration script:

``` $ cd mac; sh config.sh```

Phits and its dependencies can then be built using `mac/phits.xcodeproj`.

### Windows ###

The Windows build is known to work with Visual Studio 2019. The process is not yet fully automated.

Open a Developer Command Prompt for Visual Studio, and build and install the `RelWithDebInfo` configurations of `external/cfitsio` and
`external/CCfits` using CMake and msbuild:

```
C:\Users\cek\phits\external\cfitsio> mkdir build && cd build
C:\Users\cek\phits\external\cfitsio\build> cmake .. -DCMAKE_INSTALL_PREFIX=. -DBUILD_SHARED_LIBS=OFF
C:\Users\cek\phits\external\cfitsio\build> msbuild /p:Configuration=RelWithDebInfo /p:Platform=x64 cfitsio.sln
C:\Users\cek\phits\external\cfitsio\build> msbuild /p:Configuration=RelWithDebInfo /p:Platform=x64 install.vcxproj
```

```
C:\Users\cek\phits\external\CCfits> mkdir build && cd build
C:\Users\cek\phits\external\CCfits\build> cmake .. -DCFITSIO_INCLUDE_DIR=..\..\cfitsio\build\include -DCFITSIO_LIBRARY=..\..\cfitsio\build\lib\cfitsio.lib
C:\Users\cek\phits\external\CCfits\build> msbuild /p:Configuration=RelWithDebInfo /p:Platform=x64 ccfits.sln
C:\Users\cek\phits\external\CCfits\build> msbuild /p:Configuration=RelWithDebInfo /p:Platform=x64 install.vcxproj
```

Next, modify the phits project to link against an external version of the [zlib](https://zlib.net) static library.
Open `win\phits.sln` and under Phits->Properties->Linker->General, make sure that the folder containing
`zlibstatic.a` is specified. Note that Phits is built using the multithreaded, static (non-DLL) runtime
library (i.e., `/MT`); make sure that zlib is built with the same option.

Finally, build the `Release` configuration of Phits using msbuild or Visual Studio.

## Installation ##

The plug-in installation location depends on your version of Photoshop. Normally, you will install the
plug-in files in a `Plug-ins` folder located in the main Photoshop installation folder, or
in a "Support Files/Plug-ins" folder under the main Photoshop Elements installation folder.

If installation is successful, there should be a "FITS..." entry in Photoshop's "About Plug-Ins" menu.

### macOS ###

#### Photoshop ####
Locate the Photoshop installation folder, for example `/Applications/Adobe Photoshop 2022`.
If it doesn't already exist, create a `Plug-ins` folder in the Photoshop installation folder.
Copy the "Phits.plugin" bundle to the `Plug-ins` folder.


#### Photoshop Elements ###
Locate the Photoshop Elements installation folder, for example `/Applications/Adobe Photoshop Elements 2022`.
In the "Support Files" folder, there should be a "Plugin-ins" folder.
Copy the "Phits.plugin" bundle to the `Plug-ins` folder.
Upon launching Elements, you may be prompted to confirm that the non-Adobe plugin should be loaded.

### Windows ###

#### Photoshop ####
Locate the Photoshop installation folder, for example `C:/Program Files/Adobe/Adobe Photoshop 2022`.
If it doesn't already exist, create a `Plug-ins` folder in the Photoshop installation folder.
Copy the `Phits.8bi` file to the `Plug-ins` folder.


#### Photoshop Elements ####
Locate the Photoshop Elements installation folder, for
example `C:/Program Files/Adobe/Adobe Photoshop Elements 2022`.
In the "Support Files" folder, there should be a "Plugin-ins" folder.
Copy the `Phits.8bi` file to the `Plug-ins` folder.
Upon launching Elements, you may be prompted to confirm that the non-Adobe plugin should be loaded.

## Troubleshooting ##

To generate a log file for troubleshooting, or for reporting a bug, create a `PHITS_LOG` environment variable containing the
path to which the log should be written, and then re-start Photoshop.

On macOS, setting the environment variable can be achieved from Terminal.app by running, for example:

```$ launchctl setenv PHITS_LOG /tmp/phits.log```.

## License ##

Phits is licensed under the Apache License, Version 2.0
