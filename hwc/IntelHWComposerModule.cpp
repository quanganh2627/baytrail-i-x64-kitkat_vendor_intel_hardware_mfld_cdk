/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <IntelHWComposer.h>

static void dump_layer(hwc_layer_t const* l)
{

}

static int hwc_prepare(hwc_composer_device_t *dev, hwc_layer_list_t* list)
{
    int status = 0;

    LOGV("%s\n", __func__);

    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (!hwc) {
        LOGE("%s: Invalid HWC device\n", __func__);
        status = -EINVAL;
        goto prepare_out;
    }

    if (hwc->prepare(list) == false) {
        status = -EINVAL;
        goto prepare_out;
    }
prepare_out:
    return 0;
}

static int hwc_set(hwc_composer_device_t *dev,
                   hwc_display_t dpy,
                   hwc_surface_t sur,
                   hwc_layer_list_t* list)
{
    int status = 0;

    LOGV("%s\n", __func__);

    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (!hwc) {
        LOGE("%s: Invalid HWC device\n", __func__);
        status = -EINVAL;
        goto set_out;
    }

    if (!list) {
        LOGE("%s: Invalid layer list\n", __func__);
        status = -EINVAL;
        goto set_out;
    }

    if (hwc->commit(dpy, sur, list) ==  false) {
        LOGE("%s: failed to commit\n", __func__);
        status = HWC_EGL_ERROR;
        goto set_out;
    }

set_out:
    return 0;
}

static void hwc_dump(struct hwc_composer_device *dev, char *buff, int buff_len)
{
    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (hwc)
       hwc->dump(buff, buff_len, 0);
}

void hwc_registerProcs(struct hwc_composer_device* dev,
                       hwc_procs_t const* procs)
{
    LOGV("%s\n", __func__);

    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    if (!hwc) {
        LOGE("%s: Invalid HWC device\n", __func__);
        return;
    }

    hwc->registerProcs(procs);
}

static int hwc_device_close(struct hw_device_t *dev)
{
#if 0
    IntelHWComposer *hwc = static_cast<IntelHWComposer*>(dev);

    LOGD("%s\n", __func__);

    delete hwc;
#endif
    return 0;
}

/*****************************************************************************/

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;

    LOGV("%s: name %s\n", __func__, name);

    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        IntelHWComposer *hwc = new IntelHWComposer();
        if (!hwc) {
            LOGE("%s: No memory\n", __func__);
            status = -ENOMEM;
            goto hwc_init_out;
        }

        /* initialize our state here */
        if (hwc->initialize() == false) {
            LOGE("%s: failed to intialize HWCompower\n", __func__);
            status = -EINVAL;
            goto hwc_init_out;
        }

        /* initialize the procs */
        hwc->hwc_composer_device_t::common.tag = HARDWARE_DEVICE_TAG;
        hwc->hwc_composer_device_t::common.version = 1;
        hwc->hwc_composer_device_t::common.module =
            const_cast<hw_module_t*>(module);
        hwc->hwc_composer_device_t::common.close = hwc_device_close;

        hwc->hwc_composer_device_t::prepare = hwc_prepare;
        hwc->hwc_composer_device_t::set = hwc_set;
        hwc->hwc_composer_device_t::dump = hwc_dump;
        hwc->hwc_composer_device_t::registerProcs = hwc_registerProcs;

        *device = &hwc->hwc_composer_device_t::common;
        status = 0;
    }
hwc_init_out:
    return status;
}

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Intel Hardware Composer",
        author: "Intel UMSE",
        methods: &hwc_module_methods,
    }
};
