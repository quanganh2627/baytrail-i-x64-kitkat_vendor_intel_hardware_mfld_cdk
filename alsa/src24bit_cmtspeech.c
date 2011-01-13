/**********************************************************************
* src.c
* Created: Nov 30, 2009
* Copyright: Intel Corporation
*
* FIR filter based sample rate converters. Currently supports:
* 	8kHz --> 48kHz upsampling
* 	48kHz--> 8kHz downsampling
*
* HISTORY:
* 2009-11-30: Created           By: Niels Nielsen - nnielse1
**********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/timeb.h>
#include <time.h>
#include <math.h>

#ifndef H_INCLUDED
#include "src24bit_cmtspeech.h"
#endif

/**********************************************************************
* Offers the following sample rate converters:
*       Upsamling 8kHz->48kHZ	: SRCUpsamp8to48()
*       Downsampling 48kHz->8kHZ	: SRCDownsamp48to8()
*       Upsamling 8kHz->48kHZ	: SRCUpsamp8to48NoAntialiasing()
*       Downsampling 48kHz->8kHZ	: SRCDownsamp48to8NoAntialiasing()
*       Upsamling 16kHz->48kHZ	: SRCUpsamp16to48()
*       Downsampling 48kHz->16kHZ	: SRCDownsamp48to16()
*       Upsamling 8kHz->16kHZ	: SRCUpsamp8to16()
*       Downsampling 16kHz->8kHZ	: SRCDownsamp16to8()
**********************************************************************/

/**********************************************************************
* Generic function description:
* int SRCxxxx(VoiceStruct* InData, VoiceStruct* OutData, int NbrSamples)
*		InData	: Input data structure, circular buffer with pointers
*		OutData	: Outputdata structure, circular buffer with pointers
*		NbrSamples	: Number of input samples to process
*
* 	struct VoiceStruct {
*		int Voice[4*BUFSIZExxKHZ];
*		int BufSize;
*		int tptr;
*		int hptr;
*		};
*
* The voice buffer has to be big enough to contain the old data used
* for the FIR filtering.
*
* At initialization all signal buffers must be filled with zeros in
* order to secure that the filters don't use random data for the
* history (could cause a very unpleasant listener experience).
*
* For downsampling functions the number of samples (NbrSamples) in
* the InData->Voice buffer must be a multiplum of the conversion
* rate. E.g. for 48->8kHz the number of input samples must be a
* multiplum of 6. If not the algorithm will loose sync between the
* two streams.
*
**********************************************************************/

/**********************************************************************
* Coefficients for 8kHz<->48kHz antialiasing filter
**********************************************************************/
#define FILT_8VS48_LEN 102
#define NBR_UPSAMP_8TO48_MULT 17
#define UPSAMP_8TO48_RATIO 6

const int Filt8vs48[FILT_8VS48_LEN] = {
12,16,17,16,12,4,-5,-17,-28,-37,
-40,-36,-22,0,28,58,83,97,94,71,
27,-30,-96,-156,-197,-207,-177,-106,0,127,
254,356,407,388,289,112,-123,-384,-627,-800,
-855,-752,-467,0,628,1371,2164,2931,3593,4078,
4336,4336,4078,3593,2931,2164,1371,628,0,-467,
-752,-855,-800,-627,-384,-123,112,289,388,407,
356,254,127,0,-106,-177,-207,-197,-156,-96,
-30,27,71,94,97,83,58,28,0,-22,
-36,-40,-37,-28,-17,-5,4,12,16,17,
16,12};


/**********************************************************************
* Function : TSRCHandle* SRCInitUpsamp8to48(int InputBufferSize,
*                                   int OutputBufferSize)
*
*----------------------------------------------------------------------
* Description : Initialize 8kHz->48kHZ upsampling function
*
**********************************************************************/
TSRCHandle16in_24out* SRCInitUpsamp8to48(short* InpBasePtr, int InputBufferSize, int OutputBufferSize)
{
	int i;
	int* iptr;

	TSRCHandle16in_24out *SRCHandle = malloc(sizeof(TSRCHandle16in_24out));

	SRCHandle->InputBuf = InpBasePtr;
	SRCHandle->InputSize = InputBufferSize;
	SRCHandle->InputWrapPtr = SRCHandle->InputBuf+InputBufferSize;
	SRCHandle->InputRPtr = SRCHandle->InputBuf;
	SRCHandle->InputWPtr = SRCHandle->InputBuf;
/*
	sptr = SRCHandle->InputBuf;
	for (i=0; i<InputBufferSize; i++)
		*sptr = 0;
*/
	SRCHandle->OutputBuf = malloc(OutputBufferSize*sizeof(int));
	SRCHandle->OutputSize = OutputBufferSize;
	SRCHandle->OutputWrapPtr = SRCHandle->OutputBuf+OutputBufferSize;
	SRCHandle->OutputRPtr = SRCHandle->OutputBuf;
	SRCHandle->OutputWPtr = SRCHandle->OutputBuf;
	iptr = SRCHandle->OutputBuf;
	for (i=0; i<OutputBufferSize; i++)
		*iptr = 0;

	return SRCHandle;
}

/**********************************************************************
* Function : void SRCExitUpsamp8to48(TSRCHandle* SRCHandle)
*
*----------------------------------------------------------------------
* Description : Clean up after 8-->48kHz upsampling function
*
**********************************************************************/
void SRCExitUpsamp8to48(TSRCHandle16in_24out *SRCHandle)
{
//	free(SRCHandle->InputBuf);
	free(SRCHandle->OutputBuf);
	free(SRCHandle);
}

/**********************************************************************
* Function : short* SRCGetInputWritePtr(TSRCHandle *SRCHandle,
*                                       int PostIncrementValue);
*
*----------------------------------------------------------------------
* Description : Get a pointer to write data into input buffer for
* sample rate converter
*
**********************************************************************/
short* SRCGetInputWritePtrUpsamp8to48(TSRCHandle16in_24out *SRCHandle, int PostIncrementValue)
{
	short* sptr;
	sptr = SRCHandle->InputWPtr;
	SRCHandle->InputWPtr+=PostIncrementValue;
	if (SRCHandle->InputWPtr>=SRCHandle->InputWrapPtr)
		SRCHandle->InputWPtr=SRCHandle->InputBuf;
	return sptr;
}

/**********************************************************************
* Function : short* SRCGetOutputReadPtr(TSRCHandle *SRCHandle,
*                                       int PostIncrementValue);
*
*----------------------------------------------------------------------
* Description : Get a pointer to read data from output buffer for
* sample rate converter
*
**********************************************************************/
int* SRCGetOutputReadPtrUpsamp8to48(TSRCHandle16in_24out *SRCHandle, int PostIncrementValue)
{
	int* sptr;
	sptr = SRCHandle->OutputRPtr;
	SRCHandle->OutputRPtr+=PostIncrementValue;
	if (SRCHandle->OutputRPtr>=SRCHandle->OutputWrapPtr)
		SRCHandle->OutputRPtr=SRCHandle->OutputBuf;
	return sptr;
}

/**********************************************************************
* Function : void SRCUpsamp8to48(TSRCHandle *SRCHandle,
*                                int NbrSamples);
*----------------------------------------------------------------------
* Description : Upsamling 8kHz->48kHZ using FIR antialisaing filter
*
**********************************************************************/
void SRCUpsamp8to48(TSRCHandle16in_24out *SRCHandle, short* InpBufPtr, int NbrSamples)
{
	int i, j, k, LoopCnt;
	int res32;
	long long int res64;
	short* Buf8 = InpBufPtr;
	short* Buf8Ptr;
	short* Buf8PtrOffs;
	short* Buf8BasePtr = SRCHandle->InputBuf;
	short* Buf8WrapPtr_1 = SRCHandle->InputWrapPtr-1;
	int* Buf48 = SRCHandle->OutputWPtr;
	int* FiltTapPtr;

	for (i=0; i<NbrSamples; i++) {
		Buf8PtrOffs = Buf8+i;
		if (Buf8PtrOffs>=SRCHandle->InputWrapPtr)
			Buf8PtrOffs = Buf8BasePtr+(Buf8PtrOffs-SRCHandle->InputWrapPtr);
		for (j=0; j<6; j++) {
			Buf8Ptr = Buf8PtrOffs;
			FiltTapPtr = ((int*) &Filt8vs48)+j;
			res32 = 0;
			LoopCnt = Buf8Ptr-Buf8BasePtr+1;
			if (LoopCnt>NBR_UPSAMP_8TO48_MULT)
				LoopCnt = NBR_UPSAMP_8TO48_MULT;
			for (k=0; k<LoopCnt; k++) {
				res32 = res32 + ((((short) *Buf8Ptr--)*((short) *FiltTapPtr)));
				FiltTapPtr+=UPSAMP_8TO48_RATIO;
			}
			if (--Buf8Ptr<Buf8BasePtr)
				Buf8Ptr = Buf8WrapPtr_1;
			for (; k<NBR_UPSAMP_8TO48_MULT; k++) {
				res32 = res32 + ((((short) *Buf8Ptr--)*((short) *FiltTapPtr)));
				FiltTapPtr+=UPSAMP_8TO48_RATIO;
			}
			if (--Buf8Ptr<Buf8BasePtr)
				Buf8Ptr = Buf8WrapPtr_1;
			res64 = ((long long) res32)*12;
			*Buf48 = (int) ((res64>(int) INT32MAX ? (int) INT32MAX : res64<(int) INT32MIN ? (int) INT32MIN : res64) >> 8);
			if (++Buf48>=SRCHandle->OutputWrapPtr)
				Buf48 = SRCHandle->OutputBuf;
		}
	}
/*
	SRCHandle->InputRPtr += NbrSamples;
	if (SRCHandle->InputRPtr>=SRCHandle->InputWrapPtr)
		SRCHandle->InputRPtr = Buf8BasePtr+(SRCHandle->InputRPtr-SRCHandle->InputWrapPtr);
*/
	SRCHandle->OutputWPtr = Buf48;

}

/**********************************************************************
* Function : void SRCUpsamp8to48NoAntialiasing(TSRCHandle *SRCHandle,
*                                int NbrSamples);
*----------------------------------------------------------------------
* Description : Upsamling 8kHz->48kHZ without antialisaing filter
*
**********************************************************************/
void SRCUpsamp8to48NoAntialiasing(TSRCHandle16in_24out *SRCHandle, short* InpBufPtr, int NbrSamples)
{
	int i, j;
	short sample;
	short* Buf8 = InpBufPtr;
	int* Buf48 = SRCHandle->OutputWPtr;

	for (i=0; i<NbrSamples; i++) {
		sample = *Buf8;
		if (++Buf8>=SRCHandle->InputWrapPtr)
			Buf8 = SRCHandle->InputBuf;
		for (j=0; j<6; j++) {
			*Buf48 = sample << 8;
			if (++Buf48>=SRCHandle->OutputWrapPtr)
				Buf48 = SRCHandle->OutputBuf;
		}
	}
/*
	SRCHandle->InputRPtr = Buf8;
*/
	SRCHandle->OutputWPtr = Buf48;
}

/**********************************************************************
* Function : TSRCHandle* SRCInitDownsamp48to8(int InputBufferSize,
*                                              int OutputBufferSize)
*
*----------------------------------------------------------------------
* Description : Initialize 48kHz->8kHZ downsampling function
*
**********************************************************************/
TSRCHandle24in_16out* SRCInitDownsamp48to8(int InputBufferSize, int OutputBufferSize)
{
	int i;
	short* sptr;
	int* iptr;

	TSRCHandle24in_16out *SRCHandle = malloc(sizeof(TSRCHandle24in_16out));

	SRCHandle->InputBuf = malloc(InputBufferSize*sizeof(int));
	SRCHandle->InputSize = InputBufferSize;
	SRCHandle->InputWrapPtr = SRCHandle->InputBuf+InputBufferSize;
	SRCHandle->InputRPtr = SRCHandle->InputBuf;
	SRCHandle->InputWPtr = SRCHandle->InputBuf;
	iptr = SRCHandle->InputBuf;
	for (i=0; i<InputBufferSize; i++)
		*iptr = 0;

	SRCHandle->OutputBuf = NULL; // malloc(OutputBufferSize*sizeof(short))
	SRCHandle->OutputSize = OutputBufferSize;
	SRCHandle->OutputWrapPtr = SRCHandle->OutputBuf+OutputBufferSize;
	SRCHandle->OutputRPtr = SRCHandle->OutputBuf;
	SRCHandle->OutputWPtr = SRCHandle->OutputBuf;
/*
	sptr = SRCHandle->OutputBuf;
	for (i=0; i<OutputBufferSize; i++)
		*sptr = 0;
*/
	return SRCHandle;
}

/**********************************************************************
* Function : void SRCExitDownsamp48to8(TSRCHandle* SRCHandle)
*
*----------------------------------------------------------------------
* Description : Clean up after 48-->8kHz downsampling function
*
**********************************************************************/
void SRCExitDownsamp48to8(TSRCHandle24in_16out *SRCHandle)
{
	free(SRCHandle->InputBuf);
//	free(SRCHandle->OutputBuf);
	free(SRCHandle);
}

/**********************************************************************
* Function : short* SRCGetInputWritePtr(TSRCHandle *SRCHandle,
*                                       int PostIncrementValue);
*
*----------------------------------------------------------------------
* Description : Get a pointer to write data into input buffer for
* sample rate converter
*
**********************************************************************/
int* SRCGetInputWritePtrDownsamp48to8(TSRCHandle24in_16out *SRCHandle, int PostIncrementValue)
{
	int* sptr;
	sptr = SRCHandle->InputWPtr;
	SRCHandle->InputWPtr+=PostIncrementValue;
	if (SRCHandle->InputWPtr>=SRCHandle->InputWrapPtr)
		SRCHandle->InputWPtr=SRCHandle->InputBuf;
	return sptr;
}

/**********************************************************************
* Function : short* SRCGetOutputReadPtr(TSRCHandle *SRCHandle,
*                                       int PostIncrementValue);
*
*----------------------------------------------------------------------
* Description : Get a pointer to read data from output buffer for
* sample rate converter
*
**********************************************************************/
short* SRCGetOutputReadPtrDownsamp48to8(TSRCHandle24in_16out *SRCHandle, int PostIncrementValue)
{
	short* sptr;
	sptr = SRCHandle->OutputRPtr;
	SRCHandle->OutputRPtr+=PostIncrementValue;
	if (SRCHandle->OutputRPtr>=SRCHandle->OutputWrapPtr)
		SRCHandle->OutputRPtr=SRCHandle->OutputBuf;
	return sptr;
}

/**********************************************************************
* Function : int SRCDownsamp48to8(VoiceStruct* InData,
*					   			  VoiceStruct* OutData,
*					   			  int NbrOutSamples);
*----------------------------------------------------------------------
* Description : Downsamling 48kHz->8kHZ using FIR antialisaing filter
*
**********************************************************************/
void SRCDownsamp48to8(TSRCHandle24in_16out *SRCHandle, short* OutBufPtr, int NbrOutSamples)
{
	int i, k, LoopCnt;
	long long res64;
	int* Buf48 = SRCHandle->InputRPtr;
	int* Buf48Ptr;
	int* Buf48BasePtr = SRCHandle->InputBuf;
	int* Buf48WrapPtr_1 = SRCHandle->InputWrapPtr-1;
	short* Buf8 = OutBufPtr;

	for (i=0; i<NbrOutSamples; i++) {
		Buf48Ptr = Buf48+i*DOWNSAMP_48TO8_RATIO;
		if (Buf48Ptr>=SRCHandle->InputWrapPtr)
			Buf48Ptr = Buf48BasePtr+(Buf48Ptr-SRCHandle->InputWrapPtr);
		res64 = 0;
		LoopCnt = Buf48Ptr-Buf48BasePtr+1;
		if (LoopCnt>FILT_8VS48_LEN)
			LoopCnt = FILT_8VS48_LEN;
		for (k=0; k<LoopCnt; k++) {
			res64 = res64 + (((((long long) *Buf48Ptr--))*((short) Filt8vs48[k])));
		}
		if (Buf48Ptr<Buf48BasePtr)
			Buf48Ptr = Buf48WrapPtr_1;
		for (k = LoopCnt; k<FILT_8VS48_LEN; k++) {
			res64 = res64 + (((((long long) *Buf48Ptr--))*((short) Filt8vs48[k])));
		}
		if (Buf48Ptr<Buf48BasePtr)
			Buf48Ptr = Buf48WrapPtr_1;
		*Buf8++ = (short) (res64>>31);

/*
		if (++Buf8>=SRCHandle->OutputWrapPtr)
			Buf8 = SRCHandle->OutputBuf;
*/
	}
	SRCHandle->InputRPtr += NbrOutSamples*DOWNSAMP_48TO8_RATIO;
	if (SRCHandle->InputRPtr>=SRCHandle->InputWrapPtr)
		SRCHandle->InputRPtr = SRCHandle->InputBuf+(SRCHandle->InputRPtr-SRCHandle->InputWrapPtr);
/*
	SRCHandle->OutputWPtr = Buf8;
*/
}

/**********************************************************************
* Function : int SRCDownsamp48to8NoAntialiasing(VoiceStruct* InData,
*					   			  VoiceStruct* OutData,
*					   			  int NbrOutSamples);
*----------------------------------------------------------------------
* Description : Downsamling 48kHz->8kHZ without antialisaing filter
*
**********************************************************************/
void SRCDownsamp48to8NoAntialiasing(TSRCHandle24in_16out *SRCHandle, short* OutBufPtr, int NbrOutSamples)
{
	int i;
	int* Buf48 = SRCHandle->InputRPtr;
	short* Buf8 = OutBufPtr;

	for (i=0; i<NbrOutSamples; i++) {
		*Buf8 = *Buf48;
		Buf48 += DOWNSAMP_48TO8_RATIO;
		if (Buf48>=SRCHandle->InputWrapPtr)
			Buf48 = SRCHandle->InputBuf;
		if (++Buf8>=SRCHandle->OutputWrapPtr)
			Buf8 = SRCHandle->OutputBuf;
	}
	SRCHandle->InputRPtr = Buf48;
/*
	SRCHandle->OutputWPtr = Buf8;'
*/
}

