/*
 **
 ** Copyright 2011 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 ** http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "INT_PROPERTY"

#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <sstream>
#include <utils/Log.h>

#include "IntProperty.h"

#define base CProperty

CIntProperty::CIntProperty(const string& strProperty, int32_t iDefaultValue) :
    base(strProperty, toString(iDefaultValue))
{
    errno = 0;

    _iPropertyValue = strtol(base::getValue().c_str(), NULL, 0);

    if ((errno != 0 && _iPropertyValue == 0) ||
            (errno == ERANGE && (_iPropertyValue == LONG_MAX || _iPropertyValue == LONG_MIN))) {

        LOGE("%s: conversion error=%s", __FUNCTION__, strerror(errno));
        // Returning the default value
        _iPropertyValue = iDefaultValue;
    }
}

CIntProperty::~CIntProperty()
{

}

// Command
int CIntProperty::getValue() const
{
    LOGD("%s: %d", __FUNCTION__, _iPropertyValue);
    return _iPropertyValue;
}


string CIntProperty::toString(int32_t iValue)
{
    ostringstream ostr;

    ostr << iValue;

    return ostr.str();
}
