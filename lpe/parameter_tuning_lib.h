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

#include <linux/types.h>
#include <sound/intel_sst_ioctl.h>

#define ENABLE 0x01
#define DISABLE 0x00
#define DIFFERENT 0x01
#define SAME 0x00
#define SET 0x00
#define GET 0x01
#define DONT_CHANGE_MOD_STATE 0x02

#define STREAM_HS		1
#define STREAM_IHF		2
#define STREAM_VIBRA1	3
#define STREAM_VIBRA2	4
#define STREAM_CAP		5

#define VMIC_HS_LEFT_CHANL 0x00
#define VMIC_HS_RIGHT_CHANL 0x01
#define VMIC_IHF_LEFT_CHANL 0x02
#define VMIC_IHF_RIGHT_CHANL 0x03
#define VMIC_VIBRA1_CHANL 0x04
#define VMIC_VIBRA2_CHANL 0x05
#define VMIC_DISABLED 0x06

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

/* Equalizer module parameters type*/
typedef enum lpe_param_types_equalizer {
	ALGO_PARAM_EQ_MONO_STEREO_SEL = 0x101,
	ALGO_PARAM_EQ_DIFF_SAME_2_CHAN,
	ALGO_PARAM_EQ_FIR_ENABLE_DISABLE_LEFT_MONO_SAME,
	ALGO_PARAM_EQ_IIR_ENABLE_DISABLE_LEFT_MONO_SAME,
	ALGO_PARAM_EQ_FIR_FIRST_LEFT_MONO_SAME,
	ALGO_PARAM_EQ_FIR_COEF_SIZE_LEFT_MONO_SAME,
	ALGO_PARAM_EQ_FIR_COEF_LEFT_MONO_SAME,
	ALGO_PARAM_EQ_IIR_COEF_SIZE_LEFT_MONO_SAME,
	ALGO_PARAM_EQ_IIR_COEF_LEFT_MONO_SAME,
	ALGO_PARAM_EQ_FIR_ENABLE_DISABLE_RIGHT,
	ALGO_PARAM_EQ_IIR_ENABLE_DISABLE_RIGHT,
	ALGO_PARAM_EQ_FIR_FIRST_RIGHT,
	ALGO_PARAM_EQ_FIR_COEF_SIZE_RIGHT,
	ALGO_PARAM_EQ_FIR_COEF_RIGHT,
	ALGO_PARAM_EQ_IIR_COEF_SIZE_RIGHT,
	ALGO_PARAM_EQ_IIR_COEF_RIGHT
}lpe_param_types_equalizer_t;

/* Sidetone module parameters type*/
typedef enum lpe_param_types_sidetone {
	ALGO_PARAM_STP_DL_MONO_STEREO_SEL = 0x201,
	ALGO_PARAM_STP_FIR_ENABLE_DISABLE,
	ALGO_PARAM_STP_AGC_ENABLE_DISABLE,
	ALGO_PARAM_STP_FIR_COEF_SIZE,
	ALGO_PARAM_STP_FIR_COEF,
	ALGO_PARAM_STP_ADAPTIVE_GAIN,
	ALGO_PARAM_STP_GAIN
}lpe_param_types_sidetone_t;

/* DC Removal module parameters type*/
typedef enum lpe_param_types_dcr {
	ALGO_PARAM_DCR_IIR_ENABLE_DISABLE = 0x301,
	ALGO_PARAM_DCR_IIR_COEF_SIZE,
	ALGO_PARAM_DCR_IIR_COEF,
}lpe_param_types_dcr_t;

/* Xprot module parameters type*/
typedef enum lpe_param_types_xprot {
	ALGO_PARAM_XPORT_PCM_TYPE = 0x401,
	ALGO_PARAM_XPORT_SFREQ,
	ALGO_PARAM_XPORT_CHANL_MODE,
	ALGO_PARAM_XPORT_DISP_LIMIT,
	ALGO_PARAM_XPORT_TEMP_LIMIT,
	ALGO_PARAM_XPORT_AMBIENT_TEMP,
	ALGO_PARAM_XPORT_VOL_LEVEL,
	ALGO_PARAM_XPORT_FRAME_LENGTH,
	ALGO_PARAM_XPORT_CONFIG_PARAMS
}lpe_param_types_xprot_t;

/* Remove Click module parameters type*/
typedef enum lpe_param_types_RMC {
	ALGO_PARAM_RMC_PCM_TYPE = 0x501,
	ALGO_PARAM_RMC_SFREQ,
	ALGO_PARAM_RMC_EMRGNCY_MODE,
	ALGO_PARAM_RMC_FRAME_LENGTH,
	ALGO_PARAM_RMC_CONFIG_PARAMS,
	ALGO_PARAM_RMC_CHAN_NUM
}lpe_param_types_RMC_t;

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
	__u32	type;		/*Type of the parameter*/
	__u32	size;		/*size of the parameters in the block*/
	__u8	params[0];	/*Parameters of the algorithm*/
}__attribute__ ((packed)) ipc_ia_params_block_t;

/* Function Declarations */

/**********************************************************************************************************************
Prototype:
	void prepare_module_header(snd_ppp_params_t *ppp_params, int algo_id, int str_id, int enable_status, int operation)

Parameters:
	IN: snd_ppp_params_t *ppp_params: pointer to the parameter module header
		algo_id: Algorithm ID
		str_id: Stream ID
		enable_status: Enable/Disable/Dont change
		operation: Operation, Get/Set parameter

Returns:
	None

Description:
	This function prepares the module header, this is used for both get and set type of operations, as it has the basic
	information about the module.
***********************************************************************************************************************/
void prepare_module_header(snd_ppp_params_t *ppp_params, int algo_id, int str_id, int enable_status, int operation);

/**********************************************************************************************************************
Prototype:
	void add_parameter_blocks(snd_ppp_params_t *ppp_params, int param_type, int param_length, char *param_data)

Parameters:
	IN: snd_ppp_params_t *ppp_params: pointer to the parameter module header
		param_type: module parameter type
		param_length: parameter length in bytes
		param_data: pointer to the parameter data block

Returns:
	None

Description:
	This function adds a new parameter block to the module data, this is used for set type of operation, as it has the
	specific detail of a particular parameter block.
***********************************************************************************************************************/
void add_parameter_blocks(snd_ppp_params_t *ppp_params, int param_type, int param_length, char *param_data);

/**********************************************************************************************************************
Prototype:
	int send_set_parameters(snd_ppp_params_t *ppp_params)

Parameters:
	IN: snd_ppp_params_t *ppp_params: pointer to the parameter module header

Returns:
	lerror status

Description:
	This function makes a set parameter IOCTL call after adding the required module and parameter information. To have
	a successfull call the module header is a must to add, parameter blocks are optional, in case user wants to do only
	enable/disable operation.
***********************************************************************************************************************/
int send_set_parameters(snd_ppp_params_t *ppp_params);

/**********************************************************************************************************************
Prototype:
	int recieve_get_parameters(snd_ppp_params_t *ppp_params)

Parameters:
	IN: snd_ppp_params_t *ppp_params: pointer to the parameter module header

Returns:
	lerror status

Description:
	This function makes a get parameter IOCTL call after adding the required module header. To have a successfull call
	the module header is a must to add.
***********************************************************************************************************************/
int recieve_get_parameters(snd_ppp_params_t *ppp_params);

int set_generic_config_params();

int set_runtime_params(int config_type, int str_id, int config_data_size, int config_data);

#ifdef __cplusplus
}
#endif
#endif /* __PARAM_TUNIG_APP_H__ */

