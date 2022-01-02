// SPDX-License-Identifier: Apache-2.0
//
// Copyright 2022, Craig Kolb
// Licensed under the Apache License, Version 2.0
//

#define plugInName            "FITS"
#define plugInCopyrightYear    "2021"
#define plugInDescription   "FITS file format plug-in."

#define vendorName            "Astimatist"
#define plugInAETEComment     "FITS file format module"

#define plugInSuiteID        'sdK4'
#define plugInClassID        'simP'
#define plugInEventID        typeNull // must be this

#include "PIDefines.h"
#include "PIResourceDefines.h"

#if __PIMac__
    #include "PIGeneral.r"
    #include "PIUtilities.r"
#elif defined(__PIWin__)
    #define Rez
    #include "PIGeneral.h"
    #include "PIUtilities.r"
#endif

#include "PITerminology.h"
#include "PIActions.h"

resource 'PiPL' (ResourceID, plugInName " PiPL", purgeable)
{
    {
        Kind { ImageFormat },
        Name { plugInName },
        Version { (latestFormatVersion << 16) | latestFormatSubVersion },

        Component { ComponentNumber, plugInName },

        #if Macintosh
            #if defined(__arm64__)
                CodeMacARM64 { "PluginMain" },
            #endif
            #if defined(__x86_64__)
                CodeMacIntel64 { "PluginMain" },
            #endif
        #elif MSWindows
            CodeEntryPointWin64 { "PluginMain" },
        #endif

        SupportsPOSIXIO {},

        HasTerminology { plugInClassID,
                         plugInEventID,
                         ResourceID,
                         vendorName " " plugInName },

        SupportedModes
        {
            doesSupportBitmap, doesSupportGrayScale,
            noIndexedColor, doesSupportRGBColor,
            doesSupportCMYKColor, doesSupportHSLColor,
            doesSupportHSBColor, noMultichannel,
            noDuotone, doesSupportLABColor
        },

        EnableInfo { "true" },

        PlugInMaxSize { 2147483647, 2147483647 },
        FormatMaxSize { { 32767, 32767 } },
        FormatMaxChannels { {   1, 24, 24, 24, 24, 24,
                               24, 24, 24, 24, 24, 24 } },
        FmtFileType { 'FITS', '8BIM' },
        //ReadTypes { { '8B1F', '    ' } },
        FilteredTypes { { '8B1F', '    ' } },
        ReadExtensions { { 'FITS', 'FIT ' } },
        WriteExtensions { { 'FITS', 'FIT ' } },
        FilteredExtensions { { 'FITS', 'FIT ' } },
        FormatFlags { fmtSavesImageResources,
                      fmtCanRead,
                      fmtCanWrite,
                      fmtCanWriteIfRead,
                      fmtCanWriteTransparency,
                      fmtCanCreateThumbnail },
        FormatICCFlags { iccCannotEmbedGray,
                         iccCanEmbedIndexed,
                         iccCannotEmbedRGB,
                         iccCanEmbedCMYK }
        }
};
