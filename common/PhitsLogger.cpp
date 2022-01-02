/* Copyright 2022, Craig E Kolb
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */
#include "PhitsLogger.h"
#include <iomanip>
#include <ctime>

PhitsLogger::PhitsLogger()
{
    char* logFile = getenv("PHITS_LOG");
    if (logFile != nullptr)
    {
        m_ofstream.open(logFile, std::ios::app);
    }
}

PhitsLogger::~PhitsLogger()
{
    if (m_ofstream.is_open())
    {
        m_ofstream.close();
    }
}

void PhitsLogger::log(const std::string& str)
{
    if (m_ofstream.is_open())
    {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        m_ofstream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S ");
        m_ofstream << str;
    }
}
