/*
 **
 ** Copyright 2012 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **      http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "Vibrator_instance"

#include <utils/Log.h>
#include <hardware/vibrator.h>
#include "Vibrator.h"

static CVibrator* getInstance()
{
    // Singleton is returned
    static CVibrator vibrator;

    return &vibrator;
}


static int intelVibraExists()
{
    return getInstance()->isPresent();
}

static int intelVibraOn(int duration_ms)
{
    return getInstance()->switchOn(duration_ms) ? 0 : -1;
}

static int intelVibraOff()
{
    return getInstance()->switchOff() ? 0 : -1;
}

/*===========================================================================*/
/* Intel vibrator HW module interface definition                             */
/*===========================================================================*/

static hw_module_methods_t intel_vibra_module_methods = {
    open : NULL,
};

extern "C" vibrator_module HAL_MODULE_INFO_SYM;

vibrator_module HAL_MODULE_INFO_SYM = {
    common : {
        tag           : HARDWARE_MODULE_TAG,
        version_major : 1,
        version_minor : 0,
        id            : VIBRATOR_HARDWARE_MODULE_ID,
        name          : "Intel vibrator HAL",
        author        : "Intel Corporation - Vincent Becker",
        methods       : &intel_vibra_module_methods,
        dso           : 0,
        reserved      : {0},
    },
    vibrator_exists : intelVibraExists,
    vibrator_on : intelVibraOn,
    vibrator_off : intelVibraOff,
};
