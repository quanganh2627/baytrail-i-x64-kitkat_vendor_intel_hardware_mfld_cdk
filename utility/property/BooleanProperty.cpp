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

#define LOG_TAG "BOOLEAN_PROPERTY"
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <sstream>
#include <utils/Log.h>

#include "BooleanProperty.h"

#define base CProperty

CBooleanProperty::CBooleanProperty(const string& strProperty, bool bDefaultValue) :
    base(strProperty, toString(bDefaultValue))
{
    if ((getValue() == "1") || (!strcasecmp(getValue().c_str(), "true"))) {

        _bValue = 1;
    } else {

        _bValue = 0;
    }

}

CBooleanProperty::~CBooleanProperty()
{

}

bool CBooleanProperty::isSet() const
{
    LOGD("%s: %d", __FUNCTION__, _bValue);

    return _bValue;
}

string CBooleanProperty::toString(bool bValue)
{
    ostringstream ostr;

    ostr << bValue;

    return ostr.str();
}
