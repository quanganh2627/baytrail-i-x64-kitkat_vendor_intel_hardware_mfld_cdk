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

namespace android
{

class acoustic
{
public :
    static int process_init();
    static int process_profile(uint32_t device, int beg_call);
    static int process_wake();
    static int process_suspend();

private :
    static int private_cache_profiles();
    static int private_get_profile_id(uint32_t device);
    static int private_wake(int fd);
    static int private_suspend(int fd);
    static int private_get_fw_label(int fd);

    static const uint32_t device_id_max     = 0x40;
    static const int      device_number     = 6;
    static const int      device_default    = device_number - 1;
    static const int      fw_max_label_size = 100;

    static bool           is_a1026_init;
    static int            profile_size[device_number];
    static unsigned char *i2c_cmd_device[device_number];

    static const char    *profile_name[device_number];
};

}

#endif /* __VPC_ACOUSTIC_H__ */

