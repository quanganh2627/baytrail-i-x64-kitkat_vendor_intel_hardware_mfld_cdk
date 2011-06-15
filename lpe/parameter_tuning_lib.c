/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/ioctl.h>
#include "parameter_tuning_lib.h"
#include <utils/Log.h>


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
void prepare_module_header(snd_ppp_params_t *ppp_params, int algo_id, int str_id, int enable_status, int operation)
{
	unsigned char *uptr;

	ppp_params->algo_id  = algo_id;
	ppp_params->str_id   = str_id;
	ppp_params->enable   = enable_status;
	ppp_params->reserved = operation;
	ppp_params->size 	 = sizeof(snd_ppp_params_t) - sizeof(int);

	uptr = (unsigned char *)(ppp_params);
	uptr = uptr + 2*sizeof(__u32) + 4*sizeof (__u8);
	ppp_params->params = uptr;
}

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
void add_parameter_blocks(snd_ppp_params_t *ppp_params, int param_type, int param_length, char *param_data)
{
	unsigned char *uptr;
	ipc_ia_params_block_t *pbptr;
	int temp_count=0;

	uptr = (unsigned char *)ppp_params->params + ppp_params->size - (sizeof(snd_ppp_params_t) - sizeof(int));
	pbptr = (ipc_ia_params_block_t *)uptr;
	pbptr->type=param_type;
	pbptr->size=param_length;
	memcpy(pbptr->params, param_data, param_length);

	temp_count = ((2*sizeof(__u32)) + param_length);
	ppp_params->size += temp_count;
}

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
int send_set_parameters(snd_ppp_params_t *ppp_params)
{
	int retval, handle;
	unsigned char * charp; int i;

	handle = open("/dev/intel_sst_ctrl", O_RDWR);
	if ( handle < 0){
		fprintf(stderr, "/dev/intel_sst_ctrl open failed : %s\n", strerror(errno));
		return -1;
	}

	/*
	fprintf(stderr, "Packing done, dump data \n");
	fprintf(stderr, "ppp_params Algo ID 0x%x Str ID %d, Enable %d, Size %d\n",
		ppp_params->algo_id, ppp_params->str_id, ppp_params->enable, ppp_params->size);
	charp = (unsigned char *)ppp_params;
	for (i = 0; i < (ppp_params->size+8); i++, charp++)
		fprintf(stderr, "%02x ", *charp);
	fprintf(stderr, "\n");
	*/

	retval = ioctl(handle, SNDRV_SST_SET_ALGO, ppp_params);
	if ( retval != 0){
		fprintf(stderr, "ioctl cmd SET_ALGO failed : %s\n", strerror(errno));
		retval = -1;
	}
	close(handle);
	return retval;
}

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
int recieve_get_parameters(snd_ppp_params_t *ppp_params)
{
	int retval, handle;
	unsigned char * charp;
	int i, dump_size=0;

	handle = open("/dev/intel_sst_ctrl", O_RDWR);
	if ( handle < 0){
		fprintf(stderr, "/dev/intel_sst_ctrl open failed : %s\n", strerror(errno));
		return -1;
	}

	/*
	fprintf(stderr, "ppp_params Algo ID 0x%x Str ID %d, Enable %d, Operation %d, Size %d\n",
		ppp_params->algo_id, ppp_params->str_id, ppp_params->enable, ppp_params->reserved, ppp_params->size);
	*/

	retval = ioctl(handle, SNDRV_SST_GET_ALGO, ppp_params);
	if ( retval != 0){
		fprintf(stderr, "ioctl cmd GET_ALGO failed : %s\n", strerror(errno));
		return -1;
	}
	close(handle);

	/*
	fprintf(stderr, "Dump recieved data, size=%d \n", ppp_params->size);
	dump_size = ppp_params->size-(sizeof(snd_ppp_params_t) - sizeof(__u32));
	charp = (unsigned char *)ppp_params->params;
	for (i = 0; i < dump_size; i++, charp++)
		fprintf(stderr, "%02x ", *charp);
	fprintf(stderr, "\n");
	*/

	return retval;
}

int set_generic_config_params(int config_type, int str_id, int config_data_size, char *config_data)
{
	int retval, handle;
	unsigned char *uptr;
	snd_sst_tuning_params_t *tuning_params=NULL;

	tuning_params = (snd_sst_tuning_params_t *) malloc(50+config_data_size);
	tuning_params->type = config_type;
	tuning_params->str_id = str_id;
	tuning_params->size = config_data_size;
	tuning_params->rsvd=0;

	uptr = (unsigned char *)(tuning_params);
	//uptr = uptr + sizeof(__u32) + 4*sizeof (__u8);
	uptr = uptr + sizeof(unsigned long) + 4*sizeof (__u8);
	//uptr[0] = VMIC_HS_LEFT_CHANL;
	memcpy(uptr, config_data, config_data_size);
	tuning_params->addr = uptr;

	handle = open("/dev/intel_sst_ctrl", O_RDWR);
	if ( handle < 0){
		fprintf(stderr, "/dev/intel_sst_ctrl open failed : %s\n", strerror(errno));
		free(tuning_params);
		return -1;
	}

	retval = ioctl(handle, SNDRV_SST_TUNING_PARAMS, tuning_params);
	if ( retval != 0){
		fprintf(stderr, "ioctl cmd SNDRV_SST_TUNING_PARAMS failed : %s\n", strerror(errno));
		retval = -1;
	}
	close(handle);
	free(tuning_params);
	return retval;
}


int set_runtime_params(int config_type, int str_id, int config_data_size, int config_data)
{
	int retval, handle;
       // unsigned char *uptr;
	snd_sst_tuning_params_t *tuning_params=NULL;
	__u64 *addr;

	tuning_params = (snd_sst_tuning_params_t *) malloc(sizeof(snd_sst_tuning_params_t));
	tuning_params->type = config_type;
	tuning_params->str_id = str_id;
	tuning_params->size = config_data_size;
	tuning_params->rsvd=0;

	addr = (__u64 *)malloc(sizeof(config_data));
	*addr = config_data;

	tuning_params->addr = (__u64)addr;
	handle = open("/dev/intel_sst_ctrl", O_RDWR);
	if ( handle < 0){
		fprintf(stderr, "/dev/intel_sst_ctrl open failed : %s\n", strerror(errno));
                LOGE("enter the set runtime params:open error %s",strerror(errno));
		free(addr);
		free(tuning_params);
		return -1;
	}

	retval = ioctl(handle, SNDRV_SST_SET_RUNTIME_PARAMS, tuning_params);
	if ( retval != 0){
		fprintf(stderr, "ioctl cmd SNDRV_SST_TUNING_PARAMS failed : %s\n", strerror(errno));
                LOGE("enter the set runtime params:ioctl error");
		retval = -1;
	}
	close(handle);

	free(addr);
	free(tuning_params);
	return retval;
}
