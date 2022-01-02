// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright 2022, Craig Kolb
 * Licensed under the Apache License, Version 2.0
 */
#include <vector>
#include <sstream>
#include <time.h>
#include <string>
#include <memory>
#include <assert.h>
#include "Phits.h"
#include "PIUI.h"
#include "PhitsLogger.h"
#include "PhitsMetadata.h"
#include "Timer.h"

#ifdef _WIN32
#include <io.h>
#endif

#include <CCfits/CCfits>

using namespace std;
using namespace CCfits;

extern void DoAbout(SPPluginRef plugin_ref);
extern bool DoSaveWarn(SPPluginRef pluginRef, const string& warnString);

SPBasicSuite* sSPBasic = nullptr;
PhitsLogger* gLogger = nullptr;

static void log(const string& str)
{
    if (gLogger) gLogger->log(str + "\n");
}

class PhitsPlugin
{
public:
    PhitsPlugin()
        : m_formatRecord(nullptr)
        , m_result(nullptr)
    {
    }

    void setResultPointer(int16* result) { m_result = result; }
    void setFormatRecord(FormatRecordPtr pFormatRecord)
    {
        // In theory, FormatRecordPtr can change between plugin invocations.
        m_formatRecord = pFormatRecord;
        m_formatRecord->PluginUsing32BitCoordinates = true;
        m_pluginRef = reinterpret_cast<SPPluginRef>(m_formatRecord->plugInRef);
    }
    void readPrepare(void);
    void readStart(void);
    void readContinue(void);
    void readFinish(void);
    void optionsPrepare(void);
    void optionsStart(void);
    void optionsContinue(void);
    void optionsFinish(void);
    void estimatePrepare(void);
    void estimateStart(void);
    void estimateContinue(void);
    void estimateFinish(void);
    void writePrepare(void);
    void writeStart(void);
    void writeContinue(void);
    void writeFinish(void);
    void filterFile(void);
private:
    void setErrorString(const string& str);
    string getFormatName(int fmt);

    unique_ptr<FITS> m_pFits;
    PHDU* m_pPHDU = nullptr;            // Stashed pointer to PHDU; invalidated when m_pFits is destoyed
    FormatRecordPtr m_formatRecord = nullptr;
    SPPluginRef m_pluginRef = nullptr;
    int16* m_result = nullptr;
};

static PhitsPlugin* gPlugin;

void PhitsPlugin::setErrorString(const string& str)
{
    if (m_formatRecord->errorString == nullptr || str.size() > 255)
    {
        return;
    }
    uint8_t strLen = (uint8_t)str.size();
    Str255& errStr = *m_formatRecord->errorString;
    errStr[0] = strLen;
    copy(str.c_str(), str.c_str() + strLen + 1, (char *)(errStr + 1));
    log(str);
}

string PhitsPlugin::getFormatName(int fmt)
{
    switch (fmt)
    {
        case BYTE_IMG:
            return "BYTE_IMG";
        case SHORT_IMG:
            return "SHORT_IMG";
        case FLOAT_IMG:
            return "FLOAT_IMG";
        case LONG_IMG:
            return "LONG_IMG";
        case LONGLONG_IMG:
            return "LONGLONG_IMG";
        case DOUBLE_IMG:
            return "DOUBLE_IMG";
        default:
            return "unknown";
    }
}

// Reading

void PhitsPlugin::readPrepare(void)
{
    m_formatRecord->maxData = 0;

#if __PIMac__
    m_formatRecord->pluginUsingPOSIXIO = true;
#endif
}


void PhitsPlugin::readStart(void)
{
    m_formatRecord->imageRsrcSize = 0;
    m_formatRecord->imageRsrcData = sPSHandle->New(0);

    *m_result = PSSDKSetFPos (m_formatRecord->dataFork,
                             m_formatRecord->posixFileDescriptor,
                             m_formatRecord->pluginUsingPOSIXIO,
                             fsFromStart, 0);

#ifdef _WIN32
    const int fd = _open_osfhandle(m_formatRecord->dataFork, 0);
    if (fd < 0)
    {
        setErrorString("Could not open FITS file: Failed to convert file handle to file descriptor.");
        *m_result = errReportString;
        m_pFits.reset();
        return;
    }
#else
    const int fd = m_formatRecord->posixFileDescriptor;
#endif
    log("readStart");
    // FIXME: Extract filename from metadata for better error reporting?
    string name("PhotoshopFile");
    vector<string> keys;

    try
    {
        m_pFits = make_unique<FITS>(name, RWmode::Read, false, keys, fd);
    }
    catch (const exception& e)
    {
        setErrorString("could not open FITS file : " + string(e.what()));
        *m_result = errReportString;
        m_pFits.reset();
        return;
    }
    catch (const FitsException& e)
    {
        setErrorString("could not open FITS file : " + string(e.message()));
        int status;
        log("Error messages:");
        log("---");
        do
        {
            char msg[128];
            status = fits_read_errmsg(msg);
            if (status)
            {
                log(msg);
            }
        } while (status);
        log("---");
        *m_result = errReportString;
        m_pFits.reset();
        return;
    }

    PHDU& pHDU = m_pFits->pHDU();
    m_pPHDU = &pHDU;
    m_pPHDU->readAllKeys();

    if (pHDU.axes() != 2 && pHDU.axes() != 3)
    {
        // FIXME: Should this happen in the filter phase instead?
        string errorStr;
        if (pHDU.axes() == 0)
        {
            errorStr = "the FITS file does not contain a primary image";
        }
        else
        {
            errorStr = "the primary FITS image has " + to_string(pHDU.axes()) + " axes, which is not supported";
        }
        setErrorString(errorStr);
        *m_result = errReportString;
        m_pFits.reset();
        return;
    }

    const int xres = pHDU.axis(0);
    const int yres = pHDU.axis(1);
    const int planes = pHDU.axes() > 2 ? pHDU.axis(2) : 1;
    const bool isScaled = pHDU.zero() != 0. || pHDU.scale() != 1.;

    string message("Resolution: ");
    message += to_string(xres) + "x" + to_string(yres) + "x" + to_string(planes) + ", " + to_string(pHDU.axes()) + " axes";
    log(message);

    VPoint imageSize;
    imageSize.h = xres;
    imageSize.v = yres;
    m_formatRecord->imageSize32 = imageSize;

    const int fmt = pHDU.bitpix();

    int depth = 0;
    int inDepth = 0;
    switch (fmt)
    {
        case BYTE_IMG:
            inDepth = 8;
            depth = isScaled ? 32 : 8;
            break;
        case SHORT_IMG:
            inDepth = 16;
            depth = 32;
            break;
        case FLOAT_IMG:
        case LONG_IMG:
            inDepth = 32;
            depth = 32;
        case LONGLONG_IMG:
        case DOUBLE_IMG:
            inDepth = 64;
            depth = 32;
            break;
        default:
        {
            const string fmtName = getFormatName(fmt);
            string errorStr = "FITS image is of type " + fmtName + ", which is not supported";
            setErrorString(errorStr);
            log(errorStr);
            *m_result = errReportString;
            return;
        }
    }
    message = "Input depth: " + to_string(inDepth) + " bits per channel, editing depth: " + to_string(depth) + " bits per channel.";
    log(message);

    if (*m_result)
    {
        return;
    }

    if (planes == 1)
    {
        m_formatRecord->imageMode = plugInModeGrayScale;
    }
    else if (planes >= 3)
    {
        m_formatRecord->imageMode = plugInModeRGBColor;
    }
    else
    {
        string errorStr("FITS image has " + to_string(planes) + " planes, which is not supported");
        setErrorString(errorStr);
        *m_result = errReportString;
        return;
    }
    m_formatRecord->depth = depth;
    m_formatRecord->planes = planes;
    m_formatRecord->transparencyMatting = 0;
    m_formatRecord->loPlane = 0;
    m_formatRecord->hiPlane = planes - 1;

    log("readStart end.");
}

void PhitsPlugin::readContinue(void)
{
    // FIXME: Use other read methods to read one line at at time?
    log("readContinue");
    const VPoint imageSize = m_formatRecord->imageSize32;
    const uint32_t total = 2 * imageSize.v * m_formatRecord->planes;
    uint32_t done = 0;

    uint32_t bufferSize = (imageSize.h * m_formatRecord->depth + 7) >> 3;

    Ptr pixelData = sPSBuffer->New(&bufferSize, bufferSize);
    if (pixelData == nullptr)
    {
        log("Failed to allocate scanline buffer of " + to_string(bufferSize) + " bytes.");
        *m_result = memFullErr;
        return;
    }

    m_formatRecord->colBytes = (m_formatRecord->depth + 7) >> 3;
    m_formatRecord->rowBytes = bufferSize;
    m_formatRecord->planeBytes = 0;
    m_formatRecord->data = pixelData;

    // FIXME: Currently, we leak the metadata. How can we tell when an image is closed, and the metadata can be freed?
    PhitsMetadata* pMeta = new PhitsMetadata;

    {
        Timer timeIt;

        // Initialize our stashed metadata for this file
        map<String, Keyword*> keywordMap = m_pPHDU->keyWord();
        log("Read keyword map of size " + to_string(keywordMap.size()));

        // Create new map with reallocated Keyword pointers. We do so because the original Keyword pointers will
        // be freed when the PHDU is freed, and we still need to use the map after that point (i.e., when writing the file).
        for (const auto& entry : keywordMap)
        {
            const Keyword* pKW = entry.second;
            pMeta->keywordMap.insert({ entry.first, pKW->clone() });
        }

        // Store original bitpix, bscale, bzero.
        pMeta->bitpix = m_pPHDU->bitpix();
        pMeta->bscale = m_pPHDU->scale();
        pMeta->bzero = m_pPHDU->zero();

        // FIXME: This relies on the low-level details of the FITS standard.
        pMeta->inputDepth = (uint32_t)abs((float)pMeta->bitpix);
        log("Metadata parsing: " + to_string(timeIt.GetElapsed()));

        // If bzero or bscale have non-default values we convert to float.
        // In practice this means that only byte images w/o bzero or bscale specified are not converted to float.

        log("Zero, scale = " + to_string(pMeta->bzero) + " " + to_string(pMeta->bscale));

        const int extensionCount = m_pFits->extensionCount();
        log("FITS file has extension count of " + to_string(extensionCount));
        for (int32_t i = 0; i < extensionCount; ++i)
        {
            const auto& ext = m_pFits->extension(i + 1);
            pMeta->extensionNames.push_back(ext.name());
        }
    }

    // Stash the metadata so that we can read it on file write.
    Handle h = sPSHandle->New(sizeof(PhitsMetadata));
    Boolean oldLock = FALSE;
    Ptr p = nullptr;
    sPSHandle->SetLock(h, true, &p, &oldLock);
    *(PhitsMetadata**)p = pMeta;
    sPSHandle->SetLock(h, false, &p, &oldLock);
    const OSErr tErr = m_formatRecord->resourceProcs->addProc(fitsResource, h);
    if (tErr != noErr)
    {
        log("Error adding resource: " + to_string(tErr));
    }
    // FIXME?
    //sPSHandle->Dispose(h);

    // Read the image data
    log("Copying FITS image data, using " + to_string(bufferSize) + " bytes per row, " + to_string(m_formatRecord->planes) + " planes.");
    log("Depth is " + to_string(m_formatRecord->depth));

    valarray<float> float_contents;
    valarray<uint8_t> byte_contents;

    float normScale = 1.f;
    float normOffset = 0.f;
    float minFloatVal = std::numeric_limits<float>::max();
    float maxFloatVal = std::numeric_limits<float>::lowest();

    m_formatRecord->progressProc(done, total);

    try
    {
        Timer timeIt;

        if (m_formatRecord->depth == 8)
        {
            log("Reading byte data.");
            pMeta->isNormalized = false;
            pMeta->isConverted = false;
            m_pPHDU->read(byte_contents);
        }
        else
        {
            log("Reading float data.");
            pMeta->isConverted = pMeta->bitpix != FLOAT_IMG;
            // Read a scanline at a time to improve progress reporting.
            // We have to allocate an extra scanline and copy the data into place thanks to PHDU::read() taking a valarray.
            float_contents.resize(m_formatRecord->planes * imageSize.h * imageSize.v);
            valarray<float> floatScanline(imageSize.h);
            uint32_t fcIdx = 0;
            for (uint32_t plane = 0; plane < m_formatRecord->planes; ++plane)
            {
                for (uint32_t v = 0; v < imageSize.v; ++v, fcIdx += imageSize.h)
                {
                    m_pPHDU->read(floatScanline, fcIdx + 1, imageSize.h);
                    memcpy(&float_contents[fcIdx], &floatScanline[0], bufferSize);
                    m_formatRecord->progressProc(++done, total);
                    *m_result = m_formatRecord->advanceState();
                }
            }
        }
        log("Pre-read time: " + to_string(timeIt.GetElapsed()));
    }
    catch (const exception& e)
    {
        setErrorString("could not open FITS file : " + string(e.what()));
        *m_result = errReportString;
        m_pFits.reset();
        return;
    }
    catch (const FitsException& e)
    {
        setErrorString("could not open FITS file : " + string(e.message()));
        int status = 0;
        int count = 0;
        do
        {
            char msg[128];
            status = fits_read_errmsg(msg);
            if (status)
            {
                if (count == 0)
                {
                    log("Error messages:");
                    log("---");
                }
                log(msg);
                ++count;
            }
        } while (status);
        if (count > 0)
        {
            log("---");
        }
        *m_result = errReportString;
        m_pFits.reset();
        return;
    }

    if (m_formatRecord->depth == 32)
    {
        Timer timeIt;
        uint32_t idx = 0;
        // Find min/max for normalization
        for (uint32_t plane = 0; plane < m_formatRecord->planes; ++plane)
        {
            for (uint32_t row = 0; row < imageSize.v; ++row)
            {
                for (uint32_t col = 0; col < imageSize.h; ++col, ++idx)
                {
                    minFloatVal = min(minFloatVal, float_contents[idx]);
                    maxFloatVal = max(maxFloatVal, float_contents[idx]);
                }
            }
        }
        log("Min float val: " + to_string(minFloatVal));
        log("Max float val: " + to_string(maxFloatVal));

        // If the data values fall outside of [0,1], normalize them.
        if (minFloatVal < 0.f || maxFloatVal > 1.f)
        {
            pMeta->isNormalized = true;
            normOffset = -minFloatVal;
            normScale = (maxFloatVal - minFloatVal);
            log("Normalizing float data, offset: " + to_string(normOffset) + ", divisor: " + to_string(normScale));
            normScale = 1.f / normScale;
        }
        log("Analysis time: " + to_string(timeIt.GetElapsed()));
    }

    uint32_t scanlineSize = imageSize.h * m_formatRecord->depth / 8;
    void* sourceData = (m_formatRecord->depth == 8 ? static_cast<void*>(&byte_contents[0]) : static_cast<void*>(&float_contents[0]));
    std::vector<float> floatScanline(imageSize.h);

    m_formatRecord->theRect32.left = 0;
    m_formatRecord->theRect32.right = imageSize.h;

    // Copy the values into place, performing any necessary normalization as we do so.
    {
        Timer timeIt;
        for (uint32_t plane = 0; plane < m_formatRecord->planes; ++plane)
        {
            if (*m_result != noErr) break;
            m_formatRecord->loPlane = m_formatRecord->hiPlane = plane;

            for (uint32_t row = 0; *m_result == noErr && row < imageSize.v; ++row)
            {
                m_formatRecord->theRect32.top = row;
                m_formatRecord->theRect32.bottom = row + 1;

                switch (m_formatRecord->depth)
                {
                case 8:
                    memcpy(pixelData, sourceData, bufferSize);
                    break;
                case 32:
                    if (pMeta->isNormalized)
                    {
                        float* fp = static_cast<float*>(sourceData);
                        for (uint32_t i = 0; i < imageSize.h; ++i)
                        {
                            floatScanline[i] = (normOffset + fp[i]) * normScale;
                        }
                        memcpy(pixelData, &floatScanline[0], bufferSize);
                    }
                    else
                    {
                        memcpy(pixelData, sourceData, bufferSize);
                    }
                    break;
                default:
                    assert(false);
                    break;
                }

                *m_result = m_formatRecord->advanceState();
                if (*m_result != noErr) break;
                m_formatRecord->progressProc(++done, total);
                sourceData = ((uint8_t*)sourceData) + scanlineSize;
            }
        }
        log("Processing time: " + to_string(timeIt.GetElapsed()));
    }

    log("Done copying FITS image data.");

    m_formatRecord->data = nullptr;
    sPSBuffer->Dispose(&pixelData);
}

void PhitsPlugin::readFinish(void)
{
    log("readFinish");
}

// Options

void PhitsPlugin::optionsPrepare(void)
{
    log("optionsPrepare");
    m_formatRecord->maxData = 0;

    if ((m_formatRecord->imageMode != plugInModeRGBColor && m_formatRecord->imageMode != plugInModeGrayScale))
    {
        setErrorString("image mode must be RGB or Grayscale");
        *m_result = errReportString;
    }
}

void PhitsPlugin::optionsStart(void)
{
    log("optionsStart");
    m_formatRecord->data = nullptr;

    const uint32_t cnt = m_formatRecord->resourceProcs->countProc(fitsResource);
    PhitsMetadata* pMeta = nullptr;
    if (cnt == 0)
    {
        log("No metadata for FITS file found in DoOptionsStart.");
        return;
    }
    Handle h = m_formatRecord->resourceProcs->getProc(fitsResource, 1);
    Ptr p;
    sPSHandle->SetLock(h, true, &p, nullptr);
    pMeta = *(PhitsMetadata**)p;
    sPSHandle->SetLock(h, false, &p, nullptr);

    if (pMeta->extensionNames.size() == 0 && !pMeta->isNormalized && !pMeta->isConverted)
    {
        return;
    }

    string warnString;
    if (pMeta->isConverted)
    {
        std::string formatStr = (pMeta->bitpix < 0 ? "floating-point" : "integer");
        warnString += "The " + to_string(pMeta->inputDepth) + "-bit " + formatStr;
        warnString += " data from the original FITS file has been converted to 32-bit floating-point";
        warnString += " data in the [0,1] range, and will be saved as a FITS FLOAT_IMG.\n\r";
    }
    else if (pMeta->isNormalized)
    {
        warnString += "The floating-point data from the the original FITS file has been normalized to the [0,1] range.\n\r";
    }

    string extString;
    if (pMeta->extensionNames.size() > 0)
    {
        extString += " original FITS file contained extra data in ";
        if (pMeta->extensionNames.size() == 1)
        {
            extString += "an extension named '" + pMeta->extensionNames[0] + "'";
        }
        else
        {
            extString += "extensions named ";
            for (uint32_t i = 0; i < pMeta->extensionNames.size(); ++i)
            {
                extString += "'" + pMeta->extensionNames[i] + "'";
                if (i < pMeta->extensionNames.size() - 1)
                {
                    extString += ", ";
                }
            }
        }
        extString += ". This additional data will not be saved.";
    }

    if (warnString.empty())
    {
        warnString = "The " + extString;
    }
    else if (!extString.empty())
    {
        warnString += "\n\rIn addition, the " + extString;
    }
    if (!DoSaveWarn(m_pluginRef, warnString))
    {
        *m_result = userCanceledErr;
    }
}

void PhitsPlugin::optionsContinue(void)
{
}

void PhitsPlugin::optionsFinish(void)
{
}

// Estimate

void PhitsPlugin::estimatePrepare(void)
{
    m_formatRecord->maxData = 0;
}

void PhitsPlugin::estimateStart(void)
{
    const VPoint imageSize = m_formatRecord->imageSize32;
    uint32_t rowBytes = (imageSize.h * m_formatRecord->depth + 7) >> 3;
    const int32 totalBytes = rowBytes * m_formatRecord->planes * imageSize.v;

    m_formatRecord->minDataBytes = totalBytes;
    m_formatRecord->maxDataBytes = totalBytes;
    m_formatRecord->data = nullptr;
}

void PhitsPlugin::estimateContinue(void)
{
}

void PhitsPlugin::estimateFinish(void)
{
}

// Writing

void PhitsPlugin::writePrepare(void)
{
    m_formatRecord->maxData = 0;

#if __PIMac__
    m_formatRecord->pluginUsingPOSIXIO = true;
#endif

}

void PhitsPlugin::writeStart(void)
{
    *m_result = PSSDKSetFPos(m_formatRecord->dataFork, m_formatRecord->posixFileDescriptor, m_formatRecord->pluginUsingPOSIXIO, fsFromStart, 0);

    if (*m_result != noErr) return;

    VPoint imageSize = m_formatRecord->imageSize32;

    int naxis = 3;
    long naxes[3] = { imageSize.h, imageSize.v, m_formatRecord->planes };

    valarray<unsigned char> char_data;
    valarray<unsigned short> short_data;
    valarray<float> float_data;

    unsigned char* fitsData = nullptr;
    int dataSize = 0;
    int fitsFormat = 0;
    switch (m_formatRecord->depth)
    {
        case 8:
            fitsFormat = BYTE_IMG;
            char_data.resize(imageSize.h);
            fitsData = reinterpret_cast<unsigned char *>(&char_data[0]);
            dataSize = 1;
            break;
        case 16:
            fitsFormat = USHORT_IMG;
            short_data.resize(imageSize.h);
            fitsData = reinterpret_cast<unsigned char *>(&short_data[0]);
            dataSize = 2;
            break;
        case 32:
            fitsFormat = FLOAT_IMG;
            float_data.resize(imageSize.h);
            fitsData = reinterpret_cast<unsigned char *>(&float_data[0]);
            dataSize = 4;
            break;
        default:
            // FIXME error
            log("Unsupported depth " + to_string(m_formatRecord->depth));
            *m_result = writErr;
            return;
    }

    log("Write start, " + to_string(imageSize.h) + "x" + to_string(imageSize.v) + "x" + to_string(m_formatRecord->planes) + ", " + to_string(dataSize*8) + " bits per plane.");

#ifdef _WIN32
    const int fd = _open_osfhandle(m_formatRecord->dataFork, 0);
    if (fd < 0)
    {
        setErrorString("Could not open FITS file: Failed to convert file handle to file descriptor.");
        *m_result = errReportString;
        return;
    }
#else
    const int fd = m_formatRecord->posixFileDescriptor;
    if (fd < 0)
    {
        log("Could not open FITS file: Invalid file descriptor provided (" + to_string(fd) + ").");
        *m_result = errReportString;
        return;
    }
#endif
    log("Creating FITS object.");
    // FIXME: Extract actual filename from metadata for improved error reporting?
    string name("PhotoshopFile");
    unique_ptr<FITS> pFitsFile;
    try
    {
        pFitsFile = make_unique<FITS>(name, fitsFormat, naxis, naxes, fd);
    }
    catch (const FitsException& e)
    {
        string msg("Failed to create FITS object: ");
        log(msg + ": " + e.message());
        *m_result = writErr;
        return;
    }
    catch (const exception& e)
    {
        string msg("Failed to create FITS object: ");
        log((msg + ": " + e.what()).c_str());
        *m_result = writErr;
        return;
    }
    catch (...)
    {
        log("Failed to create FITS object.");
        *m_result = writErr;
        return;
    }

    log("Created FITS object.");
    PHDU& pHDU = pFitsFile->pHDU();

    // Read our stashed metadata, if any, so that we can copy any keyword to the output file.
    const uint32_t cnt = m_formatRecord->resourceProcs->countProc(fitsResource);
    PhitsMetadata* pMeta = nullptr;
    if (cnt > 0)
    {
        Handle h = m_formatRecord->resourceProcs->getProc(fitsResource, 1);
        Ptr p;
        sPSHandle->SetLock(h, true, &p, nullptr);
        pMeta = *(PhitsMetadata**)p;
        sPSHandle->SetLock(h, false, &p, nullptr);
    }
    else
    {
        log("No metadata for previous FITS read found.");
    }

    if (pMeta != nullptr)
    {
        // Add all of the keywords to the output file.
        // FIXME: Preserve order in original file, rather than map order (alphabetical)
        for (const auto& entry : pMeta->keywordMap)
        {
            const Keyword* pKW = entry.second;
            string valString;
            if (pKW->keytype() == Tlogical)
            {
                // Workaround for CCfits bug
                bool boolVal;
                pKW->value(boolVal);
                valString = boolVal ? "T" : "F";
            }
            else
            {
                pKW->value(valString);
            }
            pHDU.addKey(pKW->name(), valString.c_str(), pKW->comment());
        }
    }

    // Allocate scanline buffer
    uint32_t bufferSize = (imageSize.h * m_formatRecord->depth + 7) >> 3;
    Ptr pixelData = sPSBuffer->New(&bufferSize, bufferSize);
    if (pixelData == nullptr)
    {
        *m_result = memFullErr;
        return;
    }

    m_formatRecord->colBytes = (m_formatRecord->depth + 7) >> 3;
    m_formatRecord->rowBytes = bufferSize;
    m_formatRecord->planeBytes = 0;
    m_formatRecord->data = pixelData;
    m_formatRecord->transparencyMatting = 0;

    log("Writing FITS data.");
    const int total = imageSize.h * imageSize.v;
    int curStart = 1;
    int done = 0;

    m_formatRecord->theRect32.left = 0;
    m_formatRecord->theRect32.right = imageSize.h;

    for (int plane = 0; *m_result == noErr && plane < m_formatRecord->planes; ++plane)
    {
        m_formatRecord->loPlane = m_formatRecord->hiPlane = plane;
        for (int row = 0; *m_result == noErr && row < imageSize.v; ++row)
        {
            m_formatRecord->theRect32.top = row;
            m_formatRecord->theRect32.bottom = row + 1;

            if (*m_result == noErr)
            {
                *m_result = m_formatRecord->advanceState();
            }

            if (*m_result == noErr)
            {
                memcpy(fitsData, pixelData, bufferSize);
                switch (m_formatRecord->depth)
                {
                    case 8:
                        pHDU.write(curStart, imageSize.h, char_data);
                        break;
                    case 16:
                        pHDU.write(curStart, imageSize.h, short_data);
                        break;
                    case 32:
                        pHDU.write(curStart, imageSize.h, float_data);
                        break;
                    default:
                        assert(false);
                        break;
                }
            }
            curStart += imageSize.h;
            m_formatRecord->progressProc (++done, total);
        }
    }
    log("Done writing FITS data.");

    m_formatRecord->data = nullptr;

    sPSBuffer->Dispose(&pixelData);
}

void PhitsPlugin::writeContinue(void)
{
}

void PhitsPlugin::writeFinish(void)
{
}

// Filter

void PhitsPlugin::filterFile(void)
{
    if (*m_result != noErr)
    {
        return;
    }

    log("filterFile");
#ifdef _WIN32
    const int fd = _open_osfhandle(m_formatRecord->dataFork, 0);
    if (fd < 0)
    {
        *m_result = formatCannotRead;
    }
#else
    const int fd = m_formatRecord->posixFileDescriptor;
#endif
    // FIXME: Extract filename from metadata for better error reporting?
    string name("PhotoshopFile");
    vector<string> keys;
    unique_ptr<FITS> pFitsFile;
    try
    {
        pFitsFile = make_unique<FITS>(name, RWmode::Read, false, keys, fd);
    }
    catch (const FitsException& e)
    {
        string msg("Failed to create FITS object: ");
        log(msg + ": " + e.message());
        *m_result = formatCannotRead;
        return;
    }
    catch (const exception& e)
    {
        string msg("Failed to create FITS object: ");
        log((msg + ": " + e.what()).c_str());
        *m_result = formatCannotRead;
        return;
    }
    catch (...)
    {
        string msg("Failed to create FITS object.");
        *m_result = formatCannotRead;
        return;
    }

    // FIXME: Failing the following checks doesn't prevent photoshop from
    // trying to subsequently try to read the file...?
    PHDU& pHDU = pFitsFile->pHDU();
    pHDU.readAllKeys();

    if (pHDU.axes() != 2 && pHDU.axes() != 3)
    {
        log("FITS file has unsupported axis count of " + to_string(pHDU.axes()));
        *m_result = formatCannotRead;
        return;
    }
    const int planes = pHDU.axes() > 2 ? pHDU.axis(2) : 1;
    if (planes == 2)
    {
        log("FITS image has 2 planes, which is not supported.");
        *m_result = formatCannotRead;
        return;
    }
    const int bitpix = pHDU.bitpix();
    if (bitpix != BYTE_IMG && bitpix != SHORT_IMG && bitpix != FLOAT_IMG)
    {
        log("FITS image is of unsupported type " + to_string(bitpix));
        *m_result = formatCannotRead;
    }
    log("Successfully filtered FITS image.");
}

//-------------------------------------------------------------------------------
//
//    PluginMain / main (description taken from Adobe SDK)
//
//    All calls to the plug-in module come through this routine.
//    It must be placed first in the resource.  To achieve this,
//    most development systems require this be the first routine
//    in the source.
//
//    The entrypoint will be "pascal void" for Macintosh,
//    "void" for Windows.
//
//    Inputs:
//        const int16 selector                        Host provides selector indicating
//                                                    what command to do.
//
//        FormatRecordPtr formatParamBloc                Host provides pointer to parameter
//                                                    block containing pertinent data
//                                                    and callbacks from the host.
//                                                    See PIFormat.h.
//
//    Outputs:
//        FormatRecordPtr formatParamBlock            Host provides pointer to parameter
//                                                    block containing pertinent data
//                                                    and callbacks from the host.
//                                                    See PIFormat.h.
//
//        intptr_t* data                                Use this to store a pointer to our
//                                                    global parameters structure, which
//                                                    is maintained by the host between
//                                                    calls to the plug-in.
//
//        int16* result                                Return error result or noErr.  Some
//                                                    errors are handled by the host, some
//                                                    are silent, and some you must handle.
//                                                    See PIGeneral.h.
//
//-------------------------------------------------------------------------------

DLLExport MACPASCAL void PluginMain(const int16 selector, FormatRecordPtr formatParamBlock, intptr_t* data, int16* result)
{
    try
    {
        if (gLogger == nullptr)
        {
            gLogger = new PhitsLogger;
        }

        Timer timeIt;

        if (selector == formatSelectorAbout)
        {
            AboutRecordPtr aboutRecord = reinterpret_cast<AboutRecordPtr>(formatParamBlock);
            sSPBasic = aboutRecord->sSPBasic;
            SPPluginRef pPluginRef = reinterpret_cast<SPPluginRef>(aboutRecord->plugInRef);
            DoAbout(pPluginRef);
        }
        else
        {
            if (formatParamBlock->resourceProcs == nullptr ||
                formatParamBlock->resourceProcs->countProc == nullptr ||
                formatParamBlock->resourceProcs->getProc == nullptr ||
                formatParamBlock->resourceProcs->addProc == nullptr ||
                formatParamBlock->advanceState == nullptr ||
                !formatParamBlock->HostSupports32BitCoordinates)
            {
                *result = errPlugInHostInsufficient;
                return;
            }

            sSPBasic = formatParamBlock->sSPBasic;

            if (gPlugin == nullptr)
            {
                gPlugin = new PhitsPlugin;
            }

            gPlugin->setFormatRecord(formatParamBlock);
            gPlugin->setResultPointer(result);

            switch (selector)
            {
                case formatSelectorReadPrepare:
                    gPlugin->readPrepare();
                    break;
                case formatSelectorReadStart:
                    gPlugin->readStart();
                    break;
                case formatSelectorReadContinue:
                    gPlugin->readContinue();
                    break;
                case formatSelectorReadFinish:
                    gPlugin->readFinish();
                    break;
                case formatSelectorOptionsPrepare:
                    gPlugin->optionsPrepare();
                    break;
                case formatSelectorOptionsStart:
                    gPlugin->optionsStart();
                    break;
                case formatSelectorOptionsContinue:
                    gPlugin->optionsContinue();
                    break;
                case formatSelectorOptionsFinish:
                    gPlugin->optionsFinish();
                    break;
                case formatSelectorEstimatePrepare:
                    gPlugin->estimatePrepare();
                    break;
                case formatSelectorEstimateStart:
                    gPlugin->estimateStart();
                    break;
                case formatSelectorEstimateContinue:
                    gPlugin->estimateContinue();
                    break;
                case formatSelectorEstimateFinish:
                    gPlugin->estimateFinish();
                    break;
                case formatSelectorWritePrepare:
                    gPlugin->writePrepare();
                    break;
                case formatSelectorWriteStart:
                    gPlugin->writeStart();
                    break;
                case formatSelectorWriteContinue:
                    gPlugin->writeContinue();
                    break;
                case formatSelectorWriteFinish:
                    gPlugin->writeFinish();
                    break;
                case formatSelectorFilterFile:
                    gPlugin->filterFile();
                    break;
                default:
                    break;
            }
        }

        log("Elapsed time: " + to_string(timeIt.GetElapsed()));

        // If we are done with a given phase, or we encountered an error, delete temporary data.
        if (selector == formatSelectorAbout ||
            selector == formatSelectorWriteFinish ||
            selector == formatSelectorReadFinish ||
            selector == formatSelectorOptionsFinish ||
            selector == formatSelectorEstimateFinish ||
            selector == formatSelectorFilterFile ||
            *result != noErr)
        {
            delete gPlugin;
            gPlugin = nullptr;
            delete gLogger;
            gLogger = nullptr;
        }
    }
    catch (...)
    {
        delete gPlugin;
        gPlugin = nullptr;
        delete gLogger;
        gLogger = nullptr;
        if (result != nullptr)
        {
            *result = -1;
        }
    }
}
