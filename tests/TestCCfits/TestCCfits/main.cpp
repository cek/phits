// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright 2022, Craig Kolb
 * Licensed under the Apache License, Version 2.0
 */
#include <iostream>
#include <fcntl.h>
#include <valarray>
#include <CCfits/CCfits>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace CCfits;
using namespace std;

bool testRead(const string& fileName, bool useFD)
{
    vector<string> keys;
    FITS* pFitsFile = nullptr;
    int fd = -1;

    if (useFD)
    {
#ifdef _WIN32
        _sopen_s(&fd, fileName.c_str(), O_RDONLY, _SH_DENYNO, _S_IREAD);
#else
        fd = open(fileName.c_str(), O_RDONLY);
#endif
        if (fd < 0)
        {
            cerr << "Cannot open " << fileName << " for reading." << endl;
            return false;
        }
        else
        {
            cout << "Fd is " << fd << endl;
        }

        try
        {
            pFitsFile = new FITS("testing", RWmode::Read, false, keys, fd);
        }
        catch (const exception& e)
        {
            cerr << "Failed to open FITS file: " << e.what() << endl;
            return false;
        }
#ifdef _WIN32
        _lseek(fd, 0L, SEEK_SET);
#else
        lseek(fd, 0L, SEEK_SET);
#endif
    }
    else
    {
        try
        {
            pFitsFile = new FITS(fileName, RWmode::Read, false);
        }
        catch (const exception& e)
        {
            cerr << "Failed to open FITS file: " << e.what() << endl;
            return false;
        }
        catch (const FitsException& e)
        {
            cerr << "Failed to open FITS file: " << e.message() << endl;
            char msg[128];
            std::ignore = fits_read_errmsg(msg);
            cerr << "Message: " << msg << endl;
            return false;
        }
    }

    PHDU& pHDU = pFitsFile->pHDU();
    pHDU.readAllKeys();

    cout << "Axes: " << pHDU.axes() << ", ";
    for (uint32_t i = 0; i < pHDU.axes(); ++i)
    {
        cout << pHDU.axis(i);
        if (i < pHDU.axes() - 1) cout << "x";
    }
    cout << ", ";

    std::string bitpixStr;
    switch (pHDU.bitpix())
    {
        case BYTE_IMG:
            bitpixStr = "BYTE_IMG";
            break;
        case SHORT_IMG:
            bitpixStr = "SHORT_IMG";
            break;
        case USHORT_IMG:
            bitpixStr = "USHORT_IMG!?";
            break;
        case LONG_IMG:
            bitpixStr = "LONG_IMG";
            break;
        case ULONG_IMG:
            bitpixStr = "ULONG_IMG!?";
            break;
        case LONGLONG_IMG:
            bitpixStr = "LONGLONG_IMG";
            break;
        case FLOAT_IMG:
            bitpixStr = "FLOAT_IMG";
            break;
        case DOUBLE_IMG:
            bitpixStr = "DOUBLE_IMG";
            break;
        default:
            bitpixStr = "UKNOWN_IMG";
            break;
    }
    cout << bitpixStr << endl;

    map<String, Keyword*> keywordMap = pHDU.keyWord();
    cout << "Keyword count: " << keywordMap.size() << endl;

    for (auto it = keywordMap.begin(); it != keywordMap.end(); ++it)
    {
        string name;
        string comment;
        string value;
        try
        {
            const Keyword* pKW = it->second;
            name = pKW->name();
            comment = pKW->comment();
            if (pKW->keytype() == Tlogical)
            {
                // Work around CCfits bug
                bool boolValue;
                pKW->value(boolValue);
                value = boolValue ? "T" : "F";
            }
            else
            {
                pKW->value(value);
            }
            cout << name << ": " << value << " / " << comment << endl;
        }
        catch (const std::exception& e)
        {
            cerr << "Failed to read keyword: " << e.what();
        }
        catch (const FitsException& e)
        {
            cerr << "Failed to read keyword (last read was  " << name << "): " << e.message();
        }
    }
    if (useFD)
    {
#ifdef _WIN32
        _close(fd);
#else
        close(fd);
#endif
    }
    delete pFitsFile;
    return true;
}

bool testWriteShort()
{
    int naxes = 3;
    long axes[3] = {1920, 1080, 3};
#ifdef _WIN32
    int fd;
    _sopen_s(&fd, "testshort.fits", O_CREAT | O_RDWR, _SH_DENYNO, _S_IREAD | _S_IWRITE);
#else
    int fd = open("testshort.fits", O_CREAT | O_RDWR, 00644);
#endif
    if (fd < 0)
    {
        cerr << "Cannot open test file" << endl;
        return false;
    }

    FITS* pFitsFile = new FITS(string("test"), USHORT_IMG, naxes, axes, fd);
    PHDU& pHDU = pFitsFile->pHDU();

    valarray<unsigned short> vals(axes[0]);
    unsigned short* buf = new unsigned short[axes[0]];

    for (int i = 0; i < 3; i++)
    {
        for (int x = 0; x < axes[0]; x++)
        {
            //buf[x] = 0.25f + i * 0.25;
            buf[x] = 33000;
        }
        for (int y = 0; y < axes[1]; ++y)
        {
            memcpy(&vals[0], buf, axes[0] * sizeof(short));
            pHDU.write(i * (axes[0] * axes[1]) + y * axes[0] + 1, axes[0], vals);
        }
    }
    delete pFitsFile;
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
    return true;
}

bool testWriteByte()
{
    int naxes = 3;
    long axes[3] = {1920, 1080, 3};
#ifdef _WIN32
    int fd;
    _sopen_s(&fd, "testbyte.fits", O_CREAT | O_RDWR, _SH_DENYNO, _S_IREAD | _S_IWRITE);
#else
    int fd = open("testbyte.fits", O_CREAT | O_RDWR, 00644);
#endif
    if (fd < 0)
    {
        cerr << "Cannot open test file" << endl;
        return false;
    }

    FITS* pFitsFile = new FITS(string("test"), BYTE_IMG, naxes, axes, fd);
    PHDU& pHDU = pFitsFile->pHDU();
    pHDU.scale(1);
    pHDU.zero(-12);
    valarray<uint8_t> vals(axes[0]);
    uint8_t* buf = new uint8_t[axes[0]];

    for (int i = 0; i < 3; i++)
    {
        for (int x = 0; x < axes[0]; x++)
        {
            float val = 255.f * x / 1920.f;
            buf[x] = (uint8_t)min(255.f, max(0.f, val));
        }
        for (int y = 0; y < axes[1]; ++y)
        {
            pHDU.write(i * (axes[0] * axes[1]) + y * axes[0] + 1, axes[0], vals);
        }
    }
    delete pFitsFile;
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
    return true;
}

int main(int argc, const char * argv[])
{
    if (argc > 1)
    {
        cout << "Starting read test using fd" << endl;
        if (testRead(string(argv[1]), true))
        {
            cout << "Read test using fd succeeded." << endl;
        }
        else
        {
            cout << "Read test using fd FAILED." << endl;
        }
        cout << "Starting read test, using fp" << endl;
        if (testRead(string(argv[1]), false))
        {
            cout << "Read test using fp succeeded" << endl;
        }
        else
        {
            cout << "Read read test using fp FAILED" << endl;
        }
    }
    cout << "Starting read test of nonexistent file." << endl;
    testRead("bogus.fits", false);
    cout << "Read test of nonexistent file completed." << endl;
    cout << "Starting write short test" << endl;
    testWriteShort();
    cout << "Write short test completed" << endl;
    cout << "Starting write byte test" << endl;
    testWriteByte();
    cout << "Write byte test completed" << endl;
    return 0;
}
