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

#define LOG_TAG "BT_VPC"
#include <stdio.h>
#include <utils/Log.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <bt.h>
#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"
#include "hci_vs_lib.h"

namespace android
{
#define HCI_CMD_TIMEOUT_MS 5000

int bt_pcm_enable() {
    int err, dev_id, hci_sk;

    /* get hci device id if up */
    dev_id = hci_get_route(NULL);
    if (dev_id < 0) {
        LOGE("Can't get HCI device id: %s (%d)\n",
                strerror(errno), errno);
        return -1;
    }

    /* create HCI socket to send HCI cmd */
    hci_sk = hci_open_dev(dev_id);
    if (hci_sk < 0) {
        LOGE("Can't open HCI socket: %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    /* Enable Bluetooth PCM audio path */
    err = hci_vs_btip1_1_set_fm_audio_path(hci_sk, -1, -1, AUDIO_PATH_PCM,
                                           AUDIO_PATH_DO_NOT_CHANGE, -1,
                                           HCI_CMD_TIMEOUT_MS);

    if (err < 0)
        LOGE("Can't send HCI cmd to enable BT PCM audio path on sock=0x%x %s(%d)\n", hci_sk,
                strerror(errno), errno);
    else
        LOGD("BT PCM audio path enabled");

    /* Close HCI socket */
    hci_close_dev(hci_sk);

    return err;
}

int bt_pcm_disable() {
    int err, dev_id, hci_sk;

    /* get hci device id if up */
    dev_id = hci_get_route(NULL);
    if (dev_id < 0) {
        LOGE("Can't get HCI device id: %s (%d)\n",
                strerror(errno), errno);
        return -1;
    }

    /* create HCI socket to send HCI cmd */
    hci_sk = hci_open_dev(dev_id);
    if (hci_sk < 0) {
        LOGE("Can't open HCI socket: %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    /* Disable Bluetooth audio path */
    err = hci_vs_btip1_1_set_fm_audio_path(hci_sk, -1, -1, AUDIO_PATH_NONE,
                                           AUDIO_PATH_DO_NOT_CHANGE, -1,
                                           HCI_CMD_TIMEOUT_MS);

    if (err < 0)
        LOGE("Can't send HCI cmd to disable BT audio path on sock=0x%x %s(%d)\n", hci_sk,
                strerror(errno), errno);
    else
        LOGD("BT audio path disabled");

    /* Close HCI socket */
    hci_close_dev(hci_sk);

    return err;
}

}
