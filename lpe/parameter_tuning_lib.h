/*
 ** param_tuning_lib.h - parameter tuning library interface header file
 **
 ** Copyright 2010-11 Intel Corporation
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
#ifndef __PARAM_TUNIG_APP_H__
#define __PARAM_TUNIG_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#undef LOG_TAG
#define LOG_TAG "LPEModule"
#include <utils/Log.h>
#include <linux/types.h>
#include <sound/intel_sst_ioctl.h>

#define ENABLE 0x01
#define DISABLE 0x00
#define DIFFERENT 0x01
#define SAME 0x00
#define SET 0x00
#define GET 0x01
#define DONT_CHANGE_MOD_STATE 0x02

#define STREAM_HS        1
#define STREAM_IHF       2
#define STREAM_VIBRA1    3
#define STREAM_VIBRA2    4
#define STREAM_CAP       5

#define VMIC_HS_LEFT_CHANL 0x00
#define VMIC_HS_RIGHT_CHANL 0x01
#define VMIC_IHF_LEFT_CHANL 0x02
#define VMIC_IHF_RIGHT_CHANL 0x03
#define VMIC_VIBRA1_CHANL 0x04
#define VMIC_VIBRA2_CHANL 0x05
#define VMIC_DISABLED 0x06

/* Device Types used in LPE*/
typedef enum lpe_dev_types {
    LPE_DEV_DECODE,
    LPE_DEV_HS,
    LPE_DEV_EP,
    LPE_DEV_IHF,
    LPE_DEV_VIBRA1,
    LPE_DEV_VIBRA2,
    LPE_DEV_MIC1,
    LPE_DEV_MIC2,
    LPE_DEV_MIC3,
    LPE_DEV_MIC4,
    LPE_MAX_DEV
} lpe_dev_types_t;

/* LPE Algorithm module IDs*/
typedef enum lpe_algo_types {
    LPE_ALGO_TYPE_VOL_CTRL = 0x67,
    LPE_ALGO_TYPE_MUTE_CTRL = 0x68,
    LPE_ALGO_TYPE_SIDE_TONE = 0x6A,
    LPE_ALGO_TYPE_DC_REMOVEL = 0x6B,
    LPE_ALGO_TYPE_STEREO_EQ = 0x6C,
    LPE_ALGO_TYPE_SPKR_PROT = 0x6D,
    LPE_ALGO_TYPE_AV_REMOVAL = 0x70,
    LPE_ALGO_TYPE_MONO_EQ = 0x71
}lpe_algo_types_t;

/* Stereo Equalizer module parameters type*/
typedef enum lpe_param_types_stereo_equalizer {
    ALGO_PARAM_SEQ_DIFF_SAME_2_CHAN = 0x101,
    ALGO_PARAM_SEQ_LEFT_CHANNEL_CONF,
    ALGO_PARAM_SEQ_FIR_COEF_LEFT,
    ALGO_PARAM_SEQ_IIR_COEF_LEFT,
    ALGO_PARAM_SEQ_RIGHT_CHANNEL_CONF,
    ALGO_PARAM_SEQ_FIR_COEF_RIGHT,
    ALGO_PARAM_SEQ_IIR_COEF_RIGHT
}lpe_param_types_stereo_equalizer_t;

/* Mono Equalizer module parameters type*/
typedef enum lpe_param_types_mono_equalizer {
    ALGO_PARAM_MEQ_CHANNEL_CONF = 0x151,
    ALGO_PARAM_MEQ_FIR_COEF,
    ALGO_PARAM_MEQ_IIR_COEF
}lpe_param_types_mono_equalizer_t;

/* DC Removal module parameters type*/
typedef enum lpe_param_types_dcr {
    ALGO_PARAM_DCR_IIR_ENABLE_DISABLE = 0x301,
    ALGO_PARAM_DCR_IIR_COEF,
}lpe_param_types_dcr_t;

/* volume control module parameters type */
typedef enum lpe_param_types_vol_ctrl {
    ALGO_PARAM_VOL_CTRL_STEREO_MONO = 0x601,
    ALGO_PARAM_VOL_CTRL_GAIN = 0x602
}lpe_param_types_vol_ctrl_t;

/* mute control module parameters type */
typedef enum lpe_param_types_mute_ctrl {
    ALGO_PARAM_MUTE_STEREO_MONO = 0x701,
    ALGO_PARAM_MUTE_ENABLE_DISABLE = 0x702,
    ALGO_PARAM_HALF_MUTE_ENABLE_DISABLE = 0x703,
}lpe_param_types_mute_ctrl_t;

typedef enum fw_param_type{
    LOW_LATENCY=0,
    NON_LOW_LATENCY,
    VMIC_CONFIG,
    DMA_INPUT_BUFFER,
    DMA_OUTPUT_BUFFER,
}fw_param_type_t;

/* General Config Params, vmic, etc */
typedef struct snd_sst_tuning_params snd_sst_tuning_params_t;

/* Pre and post processing params structure */
typedef struct snd_ppp_params snd_ppp_params_t;

/* Parameters block of a module */
typedef struct ipc_ia_params_block {
    __u32    type;        /*Type of the parameter*/
    __u32    size;        /*size of the parameters in the block*/
    __u8    params[0];    /*Parameters of the algorithm*/
}__attribute__ ((packed)) ipc_ia_params_block_t;

/* Function Declarations */

/**********************************************************************************************************************
Prototype:
    int prepare_module_header(snd_ppp_params_t *ppp_params, int algo_id, int dev_id, int enable_status)

Parameters:
    IN: snd_ppp_params_t *ppp_params: pointer to the parameter module header
        algo_id: Algorithm ID
        str_id: Device Type
        enable_status: Enable/Disable/Dont change

Returns:
    int: error code return

Description:
    This function prepares the module header, this is used for both get and set type of operations, as it has the basic
    information about the module.
***********************************************************************************************************************/
int prepare_module_header(snd_ppp_params_t *ppp_params, int algo_id, int dev_id, int enable_status);

/**********************************************************************************************************************
Prototype:
    int add_parameter_blocks(snd_ppp_params_t *ppp_params, int param_type, int param_length, char *param_data)

Parameters:
    IN: snd_ppp_params_t *ppp_params: pointer to the parameter module header
        param_type: module parameter type
        param_length: parameter length in bytes
        param_data: pointer to the parameter data block

Returns:
    int: error code return

Description:
    This function adds a new parameter block to the module data, this is used for set type of operation, as it has the
    specific detail of a particular parameter block.
***********************************************************************************************************************/
int add_parameter_blocks(snd_ppp_params_t *ppp_params, int param_type, int param_length, char *param_data);

/**********************************************************************************************************************
Prototype:
    int set_parameters(snd_ppp_params_t *ppp_params)

Parameters:
    IN: snd_ppp_params_t *ppp_params: pointer to the parameter module header

Returns:
    lerror status

Description:
    This function makes a set parameter IOCTL call after adding the required module and parameter information. To have
    a successfull call the module header is a must to add, parameter blocks are optional, in case user wants to do only
    enable/disable operation.
***********************************************************************************************************************/
int set_parameters(snd_ppp_params_t *ppp_params);

/**********************************************************************************************************************
Prototype:
    int get_parameters(snd_ppp_params_t *ppp_params)

Parameters:
    IN: snd_ppp_params_t *ppp_params: pointer to the parameter module header

Returns:
    lerror status

Description:
    This function makes a get parameter IOCTL call after adding the required module header. To have a successfull call
    the module header is a must to add.
***********************************************************************************************************************/
int get_parameters(snd_ppp_params_t *ppp_params);

int set_generic_config_params();

int set_runtime_params(int config_type, int str_id, int config_data_size, int config_data);

#ifdef __cplusplus
}
#endif
#endif /* __PARAM_TUNIG_APP_H__ */
