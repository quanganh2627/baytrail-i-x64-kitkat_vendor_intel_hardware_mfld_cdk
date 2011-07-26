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

#include <utils/threads.h>
#include <fcntl.h>
#include <linux/a1026.h>

#include "acoustic.h"

namespace android
{

#define ES305_DEVICE_PATH "/dev/audience_es305"

Mutex a1026_lock;

bool           acoustic::is_a1026_init = false;
int            acoustic::profile_size[device_number];
unsigned char *acoustic::i2c_cmd_device[device_number] = { NULL, };

const char *acoustic::profile_name[device_number] = {
    "close_talk.bin",         // EP
    "speaker_far_talk.bin",   // IHF
    "headset_close_talk.bin", // Headset
    "bt_hsp.bin",             // BT HSP
    "bt_carkit.bin",          // BT Car Kit
    "no_acoustic.bin"         // All other devices
};


/*---------------------------------------------------------------------------*/
/* Load profiles in cache                                                    */
/*---------------------------------------------------------------------------*/
int acoustic::private_cache_profiles()
{
    LOGD("Initialize Audience A1026 profiles cache\n");

    for (int i = 0; i < device_number; i++)
    {
        char profile_path[80] = "/system/etc/phonecall_";
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

        if (i2c_cmd_device[i] != NULL)
            free(i2c_cmd_device[i]);

        i2c_cmd_device[i] = (unsigned char*)malloc(sizeof(unsigned char) * profile_size[i]);
        if (i2c_cmd_device[i] == NULL) {
            LOGE("Could not allocate memory\n");
            fclose(fd);
            goto return_error;
        }
        else
            memset(i2c_cmd_device[i], '\0', profile_size[i]);

        int rc;
        rc = fread(&i2c_cmd_device[i][0], 1, profile_size[i], fd);
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
int acoustic::private_get_profile_id(uint32_t device)
{
    // If device above all the handeld device then put no_acoustic
    int dev_id = 0;
    if (device > device_id_max)
        dev_id = device_default;
    else {
        uint32_t local_device = device;
        while (local_device != 1) {
            local_device = local_device / 2;
            dev_id++;
        }

        // Exceptions
        // Same bin file for Headphone/Headset and BT/SCO/HEADSET/CARKIT
        switch(dev_id)
        {
        case 3 :
            LOGD("Headphone device detected, => force use of Headset device profile\n");
            dev_id = 2;
            break;
        case 4 :
            LOGD("BT SCO device detected, => force use of BT HSP device profile\n");
            dev_id = 3;
            break;
        case 5 :
           LOGD("BT SCO Headset device detected, => force use of BT HSP device profile\n");
           dev_id = 3;
           break;
        case 6 :
           LOGD("BT SCO CarKit device detected, => force use of BT CARKIT device profile\n");
           dev_id = 4;
           break;
        default :
           break;
        }
    }

    LOGD("Profile %d : size = %d, name = %s", dev_id, profile_size[dev_id], profile_name[dev_id]);

    return dev_id;
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

    rc = write(fd, &i2c_cmd_device[device_default][0], profile_size[device_default]);
    if (rc != profile_size[device_default])
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
    unsigned char firstCharLabel[4] = {0x80, 0x20, 0x00, 0x00};
    unsigned char nextCharLabel[4] = {0x80, 0x21, 0x00, 0x00};
    unsigned char label[4] = {0x00, 0x00, 0x00, 0x01};
    unsigned char *label_name;
    int i = 1, size = 4, rc;

    LOGD("Read Audience A1026 FW label\n");

    label_name = (unsigned char*)malloc(sizeof(unsigned char) * fw_max_label_size);

    // Get first build label char
    rc = write(fd, firstCharLabel, size);
    if (rc != size) {
        LOGE("A1026_WRITE_MSG (0x%.2x%.2x%.2x%.2x) error, ret = %d\n", firstCharLabel[0], firstCharLabel[1], firstCharLabel[2], firstCharLabel[3], rc);
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
        rc = write(fd, nextCharLabel, size);
        if (rc != 4) {
            LOGE("A1026_WRITE_MSG (0x%.2x%.2x%.2x%.2x) error, rc = %d\n", nextCharLabel[0], nextCharLabel[1], nextCharLabel[2], nextCharLabel[3], rc);
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
int acoustic::process_profile(uint32_t device, bool beg_call)
{
    a1026_lock.lock();
    LOGD("Audience A1026 set profile\n");

    int fd_a1026 = -1;
    int rc;
    int dev_id;

    if (!is_a1026_init) {
        LOGE("Audience A1026 not initialized.\n");
        goto return_error;
    }

    fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR);
    if (fd_a1026 < 0) {
        LOGE("Cannot open audience_a1026 device (%d)\n", fd_a1026);
        goto return_error;
    }

    if (!beg_call) {
        rc = private_wake(fd_a1026);
        if (rc) goto return_error;
    }

    dev_id = private_get_profile_id(device);

    rc = write(fd_a1026, &i2c_cmd_device[dev_id][0], profile_size[dev_id]);
    if (rc != profile_size[dev_id]) {
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

