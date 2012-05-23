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

#ifndef __VPC_ACOUSTIC_H__
#define __VPC_ACOUSTIC_H__

#include <sys/types.h>

namespace android_audio_legacy
{

class acoustic
{
public :
    static int process_init();
    static int process_profile(uint32_t device, uint32_t mode);
    static int process_wake();
    static int process_suspend();

private :
    static int private_cache_profiles();
    static int private_get_profile_id(uint32_t device, uint32_t mode);
    static int private_wake(int fd);
    static int private_suspend(int fd);
    static int private_get_fw_label(int fd);

    typedef enum {
        DEVICE_EARPIECE              = 0,
        DEVICE_SPEAKER               = 1,
        DEVICE_WIRED_HEADSET         = 2,
        DEVICE_WIRED_HEADPHONE       = 3,
        DEVICE_BLUETOOTH_SCO         = 4,
        DEVICE_BLUETOOTH_SCO_HEADSET = 5,
        DEVICE_BLUETOOTH_SCO_CARKIT  = 6,
        DEVICE_DEFAULT               = 7,
    } device_id_t;

    typedef enum {
        PROFILE_EARPIECE         = 0,
        PROFILE_SPEAKER          = 1,
        PROFILE_WIRED_HEADSET    = 2,
        PROFILE_WIRED_HEADPHONE  = 3,
        PROFILE_BLUETOOTH_HSP    = 4,
        PROFILE_BLUETOOTH_CARKIT = 5,
        PROFILE_DEFAULT          = 6,
    } profile_id_t;

    typedef enum {
        PROFILE_MODE_OFFSET_IN_CALL          = 0,
        PROFILE_MODE_OFFSET_IN_COMMUNICATION = PROFILE_DEFAULT + 1,
    } profile_mode_offset_t;

    static const int      mode_number       = 2;
    static const uint32_t device_id_max     = 0x40;
    static const int      profile_number    = mode_number * (PROFILE_DEFAULT + 1);
    static const int      fw_max_label_size = 100;

    static const size_t   profile_path_len_max = 80;

    static char           bid[80];
    static bool           is_a1026_init;
    static bool           vp_bypass_on;
    static const char *   vp_bypass_prop_name;
    static const char *   vp_fw_name_prop_name;
    static const char *   vp_profile_prefix_prop_name;
    static int            profile_size[profile_number];
    static unsigned char *i2c_cmd_profile[profile_number];

    static const char    *profile_name[profile_number];
};

}

#endif /* __VPC_ACOUSTIC_H__ */

