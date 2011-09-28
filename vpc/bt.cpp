/*
 **
 ** Copyright 2011 Intel Corporation
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

#define LOG_TAG "VPC_BT"
#include <utils/Log.h>

#include <errno.h>

#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"
#include "hci_vs_lib.h"

#include "bt.h"

namespace android
{

#define HCI_CMD_TIMEOUT_MS 5000

int bt::pcm_enable()
{
    int err = 0;
    int dev_id;
    int hci_sk = -1;

    LOGD("Enable BT PCM voice path ~~~ Entry\n");

    /* Get hci device id if up */
    if ((dev_id = hci_get_route(NULL)) < 0) {
        LOGD("  Can't get HCI device id: %s (%d)\n", strerror(errno), errno);
        LOGD("  -> Normal case if the BT chipset is disabled.\n");
        err = dev_id;
    }
    /* Create HCI socket to send HCI cmd */
    else if ((hci_sk = hci_open_dev(dev_id)) < 0) {
        LOGE("  Can't open HCI socket: %s (%d)\n", strerror(errno), errno);
        err = hci_sk;
    }
    /* Enable Bluetooth PCM audio path */
    else if ((err = hci_vs_btip1_1_set_fm_audio_path(hci_sk, -1, -1, AUDIO_PATH_PCM,
                                                     AUDIO_PATH_DO_NOT_CHANGE, -1,
                                                     HCI_CMD_TIMEOUT_MS)) < 0)
        LOGE("  Can't send HCI cmd to enable BT PCM audio path on sock: 0x%x %s(%d)\n", hci_sk,
                strerror(errno), errno);

    /* Close HCI socket */
    if (hci_sk >= 0)
        hci_close_dev(hci_sk);

    LOGD("Enable BT PCM voice path ~~~ Exit\n");

    return err;
}

int bt::pcm_disable()
{
    int err = 0;
    int dev_id;
    int hci_sk = -1;

    LOGD("Disable BT voice path ~~~ Entry\n");

    /* Get hci device id if up */
    if ((dev_id = hci_get_route(NULL)) < 0) {
        LOGD("  Can't get HCI device id: %s (%d)\n", strerror(errno), errno);
        LOGD("  -> Normal case if the BT chipset is disabled.\n");
        err = dev_id;
    }
    /* Create HCI socket to send HCI cmd */
    else if ((hci_sk = hci_open_dev(dev_id)) < 0) {
        LOGE("  Can't open HCI socket: %s (%d)\n", strerror(errno), errno);
        err = hci_sk;
    }
    /* Disable Bluetooth audio path */
    else if ((err = hci_vs_btip1_1_set_fm_audio_path(hci_sk, -1, -1, AUDIO_PATH_NONE,
                                                     AUDIO_PATH_DO_NOT_CHANGE, -1,
                                                     HCI_CMD_TIMEOUT_MS)) < 0)
        LOGE("  Can't send HCI cmd to disable BT audio path on sock: 0x%x %s(%d)\n", hci_sk,
                strerror(errno), errno);

    /* Close HCI socket */
    if (hci_sk >= 0)
        hci_close_dev(hci_sk);

    LOGD("Disable BT voice path ~~~ Exit\n");

    return err;
}

}

