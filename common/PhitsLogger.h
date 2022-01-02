// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright 2022, Craig Kolb
 * Licensed under the Apache License, Version 2.0
 */

#ifndef _PHITSLOGGER_H_
#define _PHITSLOGGER_H_

#include <fstream>
#include <iostream>

class PhitsLogger
{
public:
    PhitsLogger();
    ~PhitsLogger();
    void log(const std::string& str);
private:
    std::ofstream m_ofstream;
};

#endif /* _PHITSLOGGER_H_ */
