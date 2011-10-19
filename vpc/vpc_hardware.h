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

#ifndef __VPC_HARDWARE_H__
#define __VPC_HARDWARE_H__

#include <hardware/hardware.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum { VPC_ROUTE_OPEN, VPC_ROUTE_CLOSE } vpc_route_t;
typedef enum { VPC_TTY_OFF, VPC_TTY_ON } vpc_tty_t;
typedef enum { VPC_BT_NREC_OFF, VPC_BT_NREC_ON } vpc_bt_nrec_t;

/* VPC module struct */
#define VPC_HARDWARE_MODULE_ID "vpc"
#define VPC_HARDWARE_NAME      "vpc"

typedef struct vpc_device_t {
    hw_device_t common;

    int (*init)(void);
    int (*params)(int mode, uint32_t device);
    int (*route)(vpc_route_t);
    int (*volume)(float);
    int (*mix_disable)(int mode);
    int (*mix_enable)(int mode, uint32_t device);
    int (*tty)(vpc_tty_t);
    int (*bt_nrec)(vpc_bt_nrec_t);

} vpc_device_t;

#ifdef __cplusplus
}
#endif

#endif /* __VPC_HARDWARE_H__ */
