/**********************************************************************
* src.h
* Created: Nov 30, 2009
* Copyright: Intel Corporation
*
* FIR filter based sample rate converters
*
* HISTORY:
* 2009-11-30: Created           By: Niels Nielsen - nnielse1
**********************************************************************/
#define FILT_8VS48_LEN 			102
#define NBR_UPSAMP_8TO48_MULT 	 17
#define UPSAMP_8TO48_RATIO 		6
#define DOWNSAMP_48TO8_RATIO	6

#define INT32MAX 0x7fffffff
#define INT32MINHEX 0x80000000
#define INT32MIN -INT32MINHEX

typedef struct {
	short* InputBuf;
	int InputSize;
	short* InputWrapPtr;
	short* InputRPtr;
	short* InputWPtr;
	int *OutputBuf;
	int OutputSize;
	int* OutputWrapPtr;
	int* OutputRPtr;
	int* OutputWPtr;
} TSRCHandle16in_24out;

typedef struct {
	int *InputBuf;
	int InputSize;
	int *InputWrapPtr;
	int* InputRPtr;
	int* InputWPtr;
	short *OutputBuf;
	int OutputSize;
	short *OutputWrapPtr;
	short* OutputRPtr;
	short* OutputWPtr;
} TSRCHandle24in_16out;

TSRCHandle16in_24out* SRCInitUpsamp8to48(short* InpBasePtr, int InputBufferSize, int OutputBufferSize);
void SRCUpsamp8to48(TSRCHandle16in_24out *SRCHandle, short* InpBufPtr, int NbrSamples);
void SRCUpsamp8to48NoAntialiasing(TSRCHandle16in_24out *SRCHandle, short* InpBufPtr, int NbrSamples);
void SRCExitUpsamp8to48(TSRCHandle16in_24out *SRCHandle);
short* SRCGetInputWritePtrUpsamp8to48(TSRCHandle16in_24out *SRCHandle, int PostIncrementValue);
int* SRCGetOutputReadPtrUpsamp8to48(TSRCHandle16in_24out *SRCHandle, int PostIncrementValue);

TSRCHandle24in_16out* SRCInitDownsamp48to8(int InputBufferSize, int OutputBufferSize);
void SRCDownsamp48to8(TSRCHandle24in_16out *SRCHandle, short* OutBufPtr, int NbrSamples);
void SRCDownsamp48to8NoAntialiasing(TSRCHandle24in_16out *SRCHandle, short* OutBufPtr, int NbrSamples);
void SRCExitDownsamp48to8(TSRCHandle24in_16out *SRCHandle);
int* SRCGetInputWritePtrDownsamp48to8(TSRCHandle24in_16out *SRCHandle, int PostIncrementValue);
short* SRCGetOutputReadPtrDownsamp48to8(TSRCHandle24in_16out *SRCHandle, int PostIncrementValue);


