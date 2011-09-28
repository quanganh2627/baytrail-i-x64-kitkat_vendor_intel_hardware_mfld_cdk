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

#define LOG_TAG "VPC_Acoustic"
#include <utils/Log.h>

#include <cutils/properties.h>
#include <utils/threads.h>
#include <fcntl.h>
#include <linux/a1026.h>
#include <media/AudioSystem.h>

#include "acoustic.h"

namespace android
{

#define ES305_DEVICE_PATH "/dev/audience_es305"

Mutex a1026_lock;

bool           acoustic::is_a1026_init = false;
int            acoustic::profile_size[profile_number];
unsigned char *acoustic::i2c_cmd_profile[profile_number] = { NULL, };
char           acoustic::bid[80] = "";

const char *acoustic::profile_name[profile_number] = {
    "close_talk.bin",              // EP
    "speaker_far_talk.bin",        // IHF
    "headset_close_talk.bin",      // Headset
    "bt_hsp.bin",                  // BT HSP
    "bt_carkit.bin",               // BT Car Kit
    "no_acoustic.bin",             // All other devices
    "close_talk_voip.bin",         // EP VOIP
    "speaker_far_talk_voip.bin",   // IHF VOIP
    "headset_close_talk_voip.bin", // Headset VOIP
    "bt_hsp_voip.bin",             // BT HSP VOIP
    "bt_carkit_voip.bin",          // BT Car Kit VOIP
    "no_acoustic_voip.bin"         // All other devices VOIP
};


/*---------------------------------------------------------------------------*/
/* Load profiles in cache                                                    */
/*---------------------------------------------------------------------------*/
int acoustic::private_cache_profiles()
{
    LOGD("Initialize Audience A1026 profiles cache\n");

    for (int i = 0; i < profile_number; i++)
    {
        char profile_path[80] = "/system/etc/phonecall_";

        property_get("ro.board.id", bid, "");
        if(!strcmp(bid,"pr3")) {
            strcat(profile_path, "es305b_");
        }

        strcat(profile_path, profile_name[i]);
        FILE *fd = fopen(profile_path, "r");
        if (fd == NULL) {
            LOGE("Cannot open %s\n", profile_path);
            goto return_error;
        }

        fseek(fd, 0, SEEK_END);
        profile_size[i] = ftell(fd);
        fseek(fd, 0, SEEK_SET);

        LOGD("Profile %d : size = %d, \t path = %s", i, profile_size[i], profile_path);

        if (i2c_cmd_profile[i] != NULL)
            free(i2c_cmd_profile[i]);

        i2c_cmd_profile[i] = (unsigned char*)malloc(sizeof(unsigned char) * profile_size[i]);
        if (i2c_cmd_profile[i] == NULL) {
            LOGE("Could not allocate memory\n");
            fclose(fd);
            goto return_error;
        }
        else
            memset(i2c_cmd_profile[i], '\0', profile_size[i]);

        int rc;
        rc = fread(&i2c_cmd_profile[i][0], 1, profile_size[i], fd);
        if (rc < profile_size[i]) {
            LOGE("Error while reading config file\n");
            fclose(fd);
            goto return_error;
        }
        fclose(fd);
    }

    LOGD("Audience A1026 profiles cache OK\n");
    return 0;

return_error:

    LOGE("Audience A1026 profiles cache failed\n");
    return -1;
}

/*---------------------------------------------------------------------------*/
/* Get profile ID to access cache                                            */
/*---------------------------------------------------------------------------*/
int acoustic::private_get_profile_id(uint32_t device, uint32_t mode)
{
    int device_id = 0;
    int profile_id = PROFILE_DEFAULT;

    if (device <= device_id_max) {
        uint32_t local_device = device;
        while (local_device != 1) {
            local_device = local_device / 2;
            device_id++;
        }

        // Associate a profile to the detected device
        switch(device_id)
        {
        case DEVICE_EARPIECE :
            LOGD("Earpiece device detected, => force use of Earpiece device profile\n");
            profile_id = PROFILE_EARPIECE;
            break;
        case DEVICE_SPEAKER :
            LOGD("Speaker device detected, => force use of Speaker device profile\n");
            profile_id = PROFILE_SPEAKER;
            break;
        case DEVICE_WIRED_HEADSET :
            LOGD("Headset device detected, => force use of Headset device profile\n");
            profile_id = PROFILE_WIRED_HEADSET;
            break;
        case DEVICE_WIRED_HEADPHONE :
            LOGD("Headphone device detected, => force use of Headset device profile\n");
            profile_id = PROFILE_WIRED_HEADSET;
            break;
        case DEVICE_BLUETOOTH_SCO :
            LOGD("BT SCO device detected, => force use of BT HSP device profile\n");
            profile_id = PROFILE_BLUETOOTH_HSP;
            break;
        case DEVICE_BLUETOOTH_SCO_HEADSET :
            LOGD("BT SCO Headset device detected, => force use of BT HSP device profile\n");
            profile_id = PROFILE_BLUETOOTH_HSP;
            break;
        case DEVICE_BLUETOOTH_SCO_CARKIT :
            LOGD("BT SCO CarKit device detected, => force use of BT CARKIT device profile\n");
            profile_id = PROFILE_BLUETOOTH_CARKIT;
            break;
        default :
            LOGD("No device detected, => force use of DEFAULT device profile\n");
            profile_id = PROFILE_DEFAULT;
            break;
        }
        if (mode == AudioSystem::MODE_IN_CALL)
        {
            profile_id += PROFILE_MODE_OFFSET_IN_CALL;
        }
        else if (mode == AudioSystem::MODE_IN_COMMUNICATION)
        {
            profile_id += PROFILE_MODE_OFFSET_IN_COMMUNICATION;
        }
    }

    LOGD("Profile %d : size = %d, name = %s", profile_id, profile_size[profile_id], profile_name[profile_id]);

    return profile_id;
}

/*---------------------------------------------------------------------------*/
/* Private wake method                                                       */
/*---------------------------------------------------------------------------*/
int acoustic::private_wake(int fd)
{
    int rc;
    rc = ioctl(fd, A1026_ENABLE_CLOCK);
    if (rc)
        LOGE("Audience A1026 wake error\n");

    return rc;
}

/*---------------------------------------------------------------------------*/
/* Private suspend method                                                    */
/*---------------------------------------------------------------------------*/
int acoustic::private_suspend(int fd)
{
    int rc;

    rc = write(fd, &i2c_cmd_profile[PROFILE_DEFAULT][0], profile_size[PROFILE_DEFAULT]);
    if (rc != profile_size[PROFILE_DEFAULT])
        LOGE("Audience A1026 write error, pass-through mode failed\n");

    rc = ioctl(fd, A1026_SUSPEND);
    if (rc)
        LOGE("Audience A1026 suspend error\n");

    return rc;
}

/*---------------------------------------------------------------------------*/
/* Get Firmware label                                                        */
/*---------------------------------------------------------------------------*/
int acoustic::private_get_fw_label(int fd)
{
    const unsigned char firstCharLabelCmd[4] = {0x80, 0x20, 0x00, 0x00};
    const unsigned char nextCharLabelCmd[4] = {0x80, 0x21, 0x00, 0x00};
    unsigned char label[4] = {0x00, 0x00, 0x00, 0x01};
    unsigned char *label_name;
    int i = 1, size = 4, rc;

    LOGD("Read Audience A1026 FW label\n");

    label_name = (unsigned char*)malloc(sizeof(unsigned char) * fw_max_label_size);

    // Get first build label char
    rc = write(fd, firstCharLabelCmd, size);
    if (rc != size) {
        LOGE("A1026_WRITE_MSG (0x%.2x%.2x%.2x%.2x) error, ret = %d\n", firstCharLabelCmd[0], firstCharLabelCmd[1], firstCharLabelCmd[2], firstCharLabelCmd[3], rc);
        goto return_error;
    }
    usleep(20000);

    rc = read(fd, label, size);
    if (rc != size) {
        LOGE("A1026_READ_DATA error, ret = %d\n", rc);
        goto return_error;
    }
    label_name[0] = label[3];

    // Get next build label char
    while (label[3] && (i < fw_max_label_size))
    {
        rc = write(fd, nextCharLabelCmd, size);
        if (rc != 4) {
            LOGE("A1026_WRITE_MSG (0x%.2x%.2x%.2x%.2x) error, rc = %d\n", nextCharLabelCmd[0], nextCharLabelCmd[1], nextCharLabelCmd[2], nextCharLabelCmd[3], rc);
            goto return_error;
        }
        usleep(20000);

        rc = read(fd, label, size);
        if (rc != 4) {
            LOGE("A1026_READ_DATA error, ret = %d\n", rc);
            goto return_error;
        }
        label_name[i] = label[3];
        i++;
    }

    if (i >= fw_max_label_size) {
        LOGE("FW label invalid or too long\n");
        goto return_error;
    }

    LOGD("Audience A1026 FW label : %s\n",label_name);
    free(label_name);
    return 0;

return_error:

    LOGE("Audience A1026 read FW label failed\n");
    free(label_name);
    return -1;
}

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/
int acoustic::process_init()
{
    a1026_lock.lock();
    LOGD("Initialize Audience A1026\n");

    int fd_a1026 = -1;
    int rc;

    rc = private_cache_profiles();
    if (rc) goto return_error;

    fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR | O_NONBLOCK, 0);
    if (fd_a1026 < 0) {
        LOGE("Cannot open %s %d\n", ES305_DEVICE_PATH, fd_a1026);
        goto return_error;
    }

    LOGD("Wake Audience A1026\n");
    rc = private_wake(fd_a1026);
    if (rc) goto return_error;

    LOGD("Load Audience A1026 FW\n");
    rc = ioctl(fd_a1026, A1026_BOOTUP_INIT);
    if (rc) goto return_error;

    rc = private_get_fw_label(fd_a1026);
    if (rc) goto return_error;

    LOGD("Suspend Audience A1026\n");
    rc = private_suspend(fd_a1026);
    if (rc) goto return_error;

    LOGD("Audience A1026 init OK\n");
    is_a1026_init = true;
    close(fd_a1026);
    a1026_lock.unlock();
    return 0;

return_error:

    LOGE("Audience A1026 init failed\n");
    is_a1026_init = false;
    if (fd_a1026 >= 0)
        close(fd_a1026);
    a1026_lock.unlock();
    return -1;
}

/*---------------------------------------------------------------------------*/
/* Public wake method                                                       */
/*---------------------------------------------------------------------------*/
int acoustic::process_wake()
{
    a1026_lock.lock();
    LOGD("Wake Audience A1026\n");

    int fd_a1026 = -1;
    int rc;

    if (!is_a1026_init) {
        LOGE("Audience A1026 not initialized, nothing to wake.\n");
        goto return_error;
    }

    fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR);
    if (fd_a1026 < 0) {
        LOGE("Cannot open audience_a1026 device (%d)\n", fd_a1026);
        goto return_error;
    }

    rc = private_wake(fd_a1026);
    if (rc) goto return_error;

    LOGD("Audience A1026 wake OK\n");
    close(fd_a1026);
    a1026_lock.unlock();
    return 0;

return_error:

    LOGE("Audience A1026 wake failed\n");
    if (fd_a1026 >= 0)
        close(fd_a1026);
    a1026_lock.unlock();
    return -1;
}

/*---------------------------------------------------------------------------*/
/* Public suspend method                                                     */
/*---------------------------------------------------------------------------*/
int acoustic::process_suspend()
{
    a1026_lock.lock();
    LOGD("Suspend Audience A1026\n");

    int fd_a1026 = -1;
    int rc = -1;

    if (!is_a1026_init) {
        LOGE("Audience A1026 not initialized, nothing to suspend.\n");
        goto return_error;
    }

    fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR);
    if (fd_a1026 < 0) {
        LOGE("Cannot open audience_a1026 device (%d)\n", fd_a1026);
        goto return_error;
    }

    static const int retry = 4;
    for (int i = 0; i < retry; i++) {
        rc = private_suspend(fd_a1026);
        if (!rc)
            break;
    }

    if (rc) {
        LOGE("A1026 do hard reset to recover from error!\n");
        close(fd_a1026);
        fd_a1026 = -1;
        a1026_lock.unlock();

        rc = process_init(); // A1026 needs to do hard reset!
        a1026_lock.lock();
        if (rc) {
            LOGE("A1026 Fatal Error: Re-init A1026 Failed\n");
            goto return_error;
        }

        // After process_init(), fd_a1026 is -1
        fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR);
        if (fd_a1026 < 0) {
            LOGE("A1026 Fatal Error: unable to open A1026 after hard reset\n");
            goto return_error;
        }

        rc = private_suspend(fd_a1026);
        if (rc) {
            LOGE("A1026 Fatal Error: unable to A1026_SET_CONFIG after hard reset\n");
            close(fd_a1026);
            fd_a1026 = -1;
            goto return_error;
        }
        else
            LOGD("Set_config Ok after Hard Reset\n");
    }

    LOGD("Audience A1026 suspend OK\n");
    close(fd_a1026);
    a1026_lock.unlock();
    return 0;

return_error:

    LOGE("Audience A1026 suspend failed\n");
    if (fd_a1026 >= 0)
        close(fd_a1026);
    a1026_lock.unlock();
    return -1;
}

/*---------------------------------------------------------------------------*/
/* Set profile                                                               */
/*---------------------------------------------------------------------------*/
int acoustic::process_profile(uint32_t device, uint32_t mode)
{
    a1026_lock.lock();
    LOGD("Set Audience A1026 profile\n");

    int fd_a1026 = -1;
    int rc;
    int profile_id;

    if (!is_a1026_init) {
        LOGE("Audience A1026 not initialized.\n");
        goto return_error;
    }

    fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR);
    if (fd_a1026 < 0) {
        LOGE("Cannot open audience_a1026 device (%d)\n", fd_a1026);
        goto return_error;
    }

    profile_id = private_get_profile_id(device, mode);

    rc = write(fd_a1026, &i2c_cmd_profile[profile_id][0], profile_size[profile_id]);
    if (rc != profile_size[profile_id]) {
        LOGE("Audience write error \n");
        goto return_error;
    }

    LOGD("Audience A1026 set profile OK\n");
    close(fd_a1026);
    a1026_lock.unlock();
    return 0;

return_error:

    LOGE("Audience A1026 set profile failed\n");
    if (fd_a1026 >= 0)
        close(fd_a1026);
    a1026_lock.unlock();
    return -1;
}

}

