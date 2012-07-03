#define LOG_TAG "MediaBTService"
//#define LOG_NDEBUG 0

#include "MediaBTService.h"

#include <utils/Log.h>

#include <errno.h>

/*
 *
 *  Copyright 2012 Intel Corporation
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"
#include "hci_vs_lib.h"

namespace android {

status_t MediaBTService::init() {
    sp<IBinder> binder;
    status_t status;
    do {
        // Get the servicemanager's binder
        binder = ProcessState::self()->getContextObject(NULL);
        if (binder != 0)
            break;
        LOGW("Cannot get ServiceManager binder!!!");
        usleep(BIND_SERVICE_DELAY);
    } while(true);

    binder->linkToDeath(this);

    LOGI("MediaBT Service successfully linked to death with ServiceManager");

    status = addToServiceManager();
    if (status != NO_ERROR) {
        LOGW("addService failed with status %d.", status);
    }
    return status;
}

void MediaBTService::binderDied(const wp<IBinder>& who)
{
    (void)who;

    LOGW("ServiceManager died! Exiting process...");

    exit(-1);
}

status_t MediaBTService::addToServiceManager() {
    status_t status;
    sp<IServiceManager> sm = defaultServiceManager();

    status = sm->addService(String16("media.bt_service"), this);

    return status;
}

int MediaBTService::enableBluetoothPort(bool bEnable) {
    int err = 0;
    int dev_id;
    int hci_sk = -1;

    LOGD("%s BT PCM voice path ~~~ Entry\n", (bEnable == true) ? "Enable" : "Disable");

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
    else if ((err = hci_vs_btip1_1_set_fm_audio_path(hci_sk, -1, -1, (bEnable == true) ? AUDIO_PATH_PCM : AUDIO_PATH_NONE,
                                                     AUDIO_PATH_DO_NOT_CHANGE, -1,
                                                     MediaBTService::HCI_CMD_TIMEOUT_MS)) < 0)
        LOGE("  Can't send HCI cmd to %s BT PCM audio path on sock: 0x%x %s(%d)\n", (bEnable == true) ? "enable" : "disable",hci_sk,
                strerror(errno), errno);

    /* Close HCI socket */
    if (hci_sk >= 0)
        hci_close_dev(hci_sk);

    LOGD("%s BT PCM voice path ~~~ Exit\n", (bEnable == true) ? "Enable" : "Disable");

    return err;
}

}; // namespace android
