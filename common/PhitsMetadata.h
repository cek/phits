// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright 2022, Craig Kolb
 * Licensed under the Apache License, Version 2.0
 */
#ifndef _PHITSMETADATA_H_
#define _PHITSMETADATA_H_

#include <map>
#include <string>
#include <stdint.h>
#include <CCfits/CCfits>

struct PhitsMetadata
{
    std::map<CCfits::String, CCfits::Keyword*> keywordMap;
    int32_t bitpix = 0;
    std::vector<std::string> extensionNames;
    float bzero = 0.f;
    float bscale = 1.f;
    uint32_t inputDepth = 0;
    bool isNormalized = false;
    bool isConverted = false;
};

#endif // _PHITSMETADATA_H_
