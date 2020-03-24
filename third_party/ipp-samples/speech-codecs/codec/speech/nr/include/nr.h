/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2002-2008 Intel Corporation. All Rights Reserved.
//
//     Intel(R) Integrated Performance Primitives
//     USC - Unified Speech Codec interface library
//
//  By downloading and installing this sample, you hereby agree that the
//  accompanying Materials are being provided to you under the terms and
//  conditions of the End User License Agreement for the Intel(R) Integrated
//  Performance Primitives product previously accepted by you. Please refer
//  to the file ippEULA.rtf or ippEULA.txt located in the root directory of your Intel(R) IPP
//  product installation for more information.
//
//  Description:
//      This file contains the definitions and function prototypes for
//      the Noise Reduction example.
//
*/

#ifndef __NR_H__
#define __NR_H__

#include <ipp.h>
#include "scratchmem.h"
/*
// *****************
// General Constants
// *****************
*/
#define MAX_BLOCK_SAMPLES       320/*1024*/         /* maximum number of samples per block */
#define BETA_MIN_Q31            0x73333333   /* minimum noisy speech PSD update rate (0.9 Q31) */
#define BETA_MAX_Q31            IPP_MAX_32S  /* maximum noisy speech PSD update rate (1.0 Q31) */
#define ALPHA_MIN_Q31           0x747ae148   /* minimum clean speech PSD update mixture (0.91 Q31) */
#define ALPHA_MAX_Q31           0x7999999a   /* maximum clean speech PSD update mixture (0.95 Q31) */
#define Q15_SF                  15           /* number of shifts to adjust to Q15 base */
#define Q31_SF                  31           /* number of shifts to adjust to Q31 base */
#define INITIAL_NOISE_ESTIMATE  1            /* initial noise psd value */
#define INITIAL_SPEECH_PRESENCE 107374182    /* initial speech presence likelihood (0.05 Q31) */
#define MAG_SQR_FREQ_SF         8            /* number of right shifts applied to all mag square valued */
#define MIN_NOISY_PSD           1            /* minimum allowed value for noisy speech PSD (1 * 2^8) */
#define MIN_WIENER_GAIN         33504315     /* minimum allowed Wiener gain (-18 dB) */
#define MNS_SHIFTS_PAUSE        1            /* number of shifts to scale down coefficients during non-speech */
#define MNS_WGT_SPEECH_Q31      1610612736   /* MNS mixing weight during speech (0.75 Q31) */
#define LOW_CUTOFF_HZ           100          /* low cut-off frequency of NR filter in Hertz */
#define NOISE_AVG_WGT_Q31       1431655765   /* average noise PSD update rate (0.66 Q31) */
#define NUM_SHIFT_Q15_SQRT      (-8)         /* number of shifts needed when performing Q15 sqrt */
#define LOW_THRESHOLD_Q31       64424509     /* low threshold for uncertainty judgement (0.03 Q31) */
#define HIGH_THRESHOLD_Q31      751619276    /* low threshold for uncertainty judgement (0.3 Q31)  */
#define SMOOTH_WIN_WIDTH        19           /* smoothing fir window's width */
/*
// ****************
// Table definitions
// ****************
*/
/* Low pass filter coefficient, produced by Matlabcode round(2^31*fir1(18,0.001)) */
static const Ipp32s Smooth_FIR_Table[SMOOTH_WIN_WIDTH] = {
   17528535,    23607520,    41110864,    67928102,   100825060,   135833975,
   168732134,   195551292,   213056400,   219135885,   213056400,   195551292,
   168732134,   135833975,   100825060,    67928102,    41110864,    23607520,
   17528535};

/*
// ****************
// Type definitions
// ****************
*/
typedef Ipp32u uint;

/* NR State Structure */
typedef struct _NRstate
{
  ScratchMem_Obj Mem;
  IppMCRAState* pMCRAState;             /* pointer to noise floor estimator state structure */
  uint          cFrameSamps;            /* number of samples in a frame */
  uint          nLowBinIdx;             /* bin index of lowest frequency processed */
  Ipp32s        sumScaleFactor;         /* number of shifts to use on sums of all frequency bins */
  Ipp32s        speechPresenceQ31;      /* speech presence likelihood */
  Ipp32s        smoothSpeechPresenceQ31;/* smoothed speech presence likelihood for uncertainty judgement */
  Ipp32s*       pNoisySpeechPSD;        /* noisy speech power spectral density estimate */
  Ipp32s*       pNoisePSD;              /* noise power spectral density estimate */
  Ipp32sc*      pFilterOutput;          /* frequency domain filter output */
  Ipp16s*       pOldTimeFilterOutput16; /* time domain filter output of previous half block */
  Ipp16s*       pWindowQ15;             /* triangle window values calculated during NR_Init */
  Ipp32s        alphaQ31;               /* speech power spectral density smoothing weight */
  Ipp32s        alphaMaxQ31;            /* maximum speech power spectral density smoothing weight */
  Ipp32s        alphaMinQ31;            /* minimum speech power spectral density smoothing weight */
  Ipp32s        betaQ31;                /* noisy speech power spectral density smoothing weight */
  Ipp32s        betaMaxQ31;             /* maximum noisy speech power spectral density smoothing weight */
  Ipp32s        betaMinQ31;             /* minimum noisy speech power spectral density smoothing weight */
  IppsFFTSpec_R_16s32s* pFFTR;          /* real FFT structure */
  Ipp8u*        pBuffer;                /* FFT buffer */
#ifndef __OLD_SAMPLE
  Ipp8u*        pMemSpec;               /*Pointer to the buffer for FFTStec*/
  Ipp8u*        pFFTInitBuff;           /*Pointer to the init buffer for FFT*/
#endif
}
NRstate;

/*
// ******
// Macros
// ******
*/

/* SATURATE_32S(x,result) - Saturates an Ipp64s value to the range of Ipp32s. */

#define SATURATE_32S(x,result) \
{                              \
  if(x > IPP_MAX_32S)          \
  {                            \
    result = IPP_MAX_32S;      \
  }                            \
  else if(x < IPP_MIN_32S)     \
  {                            \
    result = IPP_MIN_32S;      \
  }                            \
  else                         \
  {                            \
    result = x;                \
  }                            \
}

/*
// MULT_SCALE_SAT_32S(src1, src2, dst, scaleFactor) - Multiplies src1 and src2. The result is saturated
//                                                    to the range of Ipp32s, shifted by scaleFactor,
//                                                    and stored in dst.
*/
#define MULT_SCALE_SAT_32S(src1, src2, dst, scaleFactor) \
{                                                        \
  Ipp64s tempResult;                                     \
                                                         \
  tempResult = (Ipp64s)(src1)*(src2);                    \
  if (scaleFactor < 0)                                   \
  {                                                      \
    if (tempResult < 0)                                  \
    {                                                    \
      tempResult = -tempResult;                          \
      tempResult <<= scaleFactor;                        \
      tempResult = -tempResult;                          \
    }                                                    \
    else                                                 \
    {                                                    \
      tempResult <<= scaleFactor;                        \
    }                                                    \
  }                                                      \
  else                                                   \
  {                                                      \
    tempResult >>= scaleFactor;                          \
  }                                                      \
  SATURATE_32S(tempResult, tempResult);                  \
  dst = (Ipp32s)tempResult;                              \
}

#define MULT_SCALE_SATQ31_32S(src1, src2, dst) \
{                                                        \
  Ipp64s tempResult;                                     \
                                                         \
  tempResult = (Ipp64s)(src1)*(src2);                    \
  tempResult >>= Q31_SF;                                 \
  SATURATE_32S(tempResult, tempResult);                  \
  dst = (Ipp32s)tempResult;                              \
}

/*
// ADD_SAT_32S(x, y, result) - Adds x and y.  The result is saturated to the range of Ipp32s and stored
//                             in result.
*/
#define ADD_SAT_32S(x, y, result)         \
{                                         \
  Ipp64s tempResult;                      \
                                          \
  tempResult = (Ipp64s)(x) + (Ipp64s)(y); \
  SATURATE_32S(tempResult, tempResult);   \
  result = (Ipp32s)tempResult;            \
}

/*
// *******************
// Function Prototypes
// *******************
*/
void NR_GetMemoryReq(Ipp32u NumSamples, Ipp32s *scratchSize);
int NR_GetLocalMemoryReq(Ipp32s *scratchSize);
void NR_Init(NRstate** pNRState, void* pMemory, uint cNumSamples, uint cSamplesPerSec);
int NR_InitBuff(NRstate* pNRState, Ipp8s* pMemory);
int NR_ReInitBuff(NRstate* pNRState);
void NR_ProcessBuffer(NRstate* nrState, Ipp16s* speechData, Ipp16s* pOutputBuf, uint useAvgNoisePSD);
void NR_FreeState(NRstate *pNRstate);

#endif /* __NR_H__ */
