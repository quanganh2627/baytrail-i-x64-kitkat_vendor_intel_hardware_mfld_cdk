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
#define LOG_TAG "PROPERTY"

#include "Property.h"
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <utils/Log.h>

#include <cutils/properties.h>

CProperty::CProperty(const string& strProperty, const string& strDefaultValue)
{
    char value[PROPERTY_VALUE_MAX];
    property_get(strProperty.c_str(), value, strDefaultValue.c_str());

    _strValue = value;
}

CProperty::~CProperty()
{

}


// Command
const string& CProperty::getValue() const
{
    LOGD("%s: %s", __FUNCTION__, _strValue.c_str());
    return _strValue;
}
