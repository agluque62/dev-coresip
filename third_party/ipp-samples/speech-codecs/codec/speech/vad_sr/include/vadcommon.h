/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2002-2007 Intel Corporation. All Rights Reserved.
//
//  Intel(R) Integrated Performance Primitives Audio Processing
//  Sample for Windows*
//
//  By downloading and installing this sample, you hereby agree that the
// accompanying Materials are being provided to you under the terms and
// conditions of the End User License Agreement for the Intel(R) Integrated
// Performance Primitives product previously accepted by you. Please refer
// to the file ippEULA.rtf or ippEULA.txt located in the root directory of your Intel(R) IPP
// product installation for more information.
//
//  Description:
//  This header source file contains the internal API structures and function prototypes
//  used by the Voice Activity Detector algorithm.
//
*/

#ifndef __VADCOMMON_H__
#define __VADCOMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

/* headers */
#include <ippdefs.h>
#include "usc_filter.h"
#define SAMPLE_FREQUENCY 16000

/* Internal constants */

/* Constants used in Energy-related computations */
#define E_INIT_NOISE_ESTIMATE_MSEC 250   /* input length in msec used to estimate initial noise energy */
#define E_FAST_ADAPT_ALPHA_Q15     163   /* noise energy adaptation parameter during speech (0.001 Q15) */
#define E_SLOW_ADAPT_ALPHA_Q15     32    /* noise energy adaptation parameter during pause (0.005 Q15) */
#define E_THRESH_OFFSET_DB_Q15     98304 /* amount above noise energy to set threshold (3 dB Q15) */
#define E_MIN_ENERGY_DB            0     /* minimum energy dB */

/* Constants used in Periodicity-related computations */
#define PER_INIT_PER_ESTIMATE_MSEC 250   /* input length in msec used to estimate initial periodicity */
#define PER_ADAPT_ALPHA_Q15        4915  /* periodicity adaptation parameter (0.15 Q15) */
#define PER_MIN_PITCH_FREQ_HZ      100   /* minimum pitch frequency in Hz */
#define PER_MAX_PITCH_FREQ_HZ      200   /* maximum pitch frequency in Hz */
#define PER_SPEECH_THRESHOLD_Q15   10485 /* 0.32 - minimum threshold for periodicity of speech*/
#define PER_DOWNSAMPLE_FACTOR      (SAMPLE_FREQUENCY /  2000)     /* factor used to downsample the bandpass filtered signal */
#define PER_DOWNSAMPLE_PHASE       0     /* relative position of the input vector in the downsampled output */

/* Onset and hang times used by the State Machine */
#define ONSET_THRESHOLD_MSEC       50    /* minimum duration in msec */
#define ENERGY_HANG_THRESH_MSEC    300   /* minimum energy hang duration in msec */
#define PER_HANG_THRESH_MSEC       500   /* minimum periodicity hang duration in msec */
#define UTT_BEG_ADJUSTMENT_MSEC    225   /* adjustment for start frame number in msec */

/* the four states of the VAD state machine */
typedef enum
{
    SILENCE = 0,
    ONSET   = 1,
    SPEECH  = 2,
    HANG    = 3
}
VADState;

/* Structure for the Energy-related state */
typedef struct _EStateStruct
{
    Ipp32s             cInitNoiseEstFrames;  /* input length in frames used to estimate an initial noise floor */
    Ipp32s             energyDB;             /* energy in dB of the current frame */
    Ipp32s             noiseFloorDB;         /* noise floor estimate in dB */
    Ipp32s             noiseThreshDB;        /* noise energy threshold in dB (noiseFloorDB + noiseEstCorrectionDB) */
    int                cFrameSamples;        /* number of samples per frame */
    int                cScaleFactor;         /* number of right shifts to use in energy calculation */
}
EStateStruct;

/* Structure for the Periodicity-related state */
typedef struct _PERStateStruct
{
    Ipp32s             cInitPerEstFrames;     /* input length in frames used to estimate an periodicity */
    Ipp32s             minPeriodSamps;        /* minimum pitch period in samples corresponding to PER_MAX_PITCH_FREQ_HZ */
    Ipp32s             maxPeriodSamps;        /* maximum pitch period in samples corresponding to PER_MIN_PITCH_FREQ_HZ */
    Ipp32s             period;                /* period of the current frame */
    Ipp16s             periodicityQ15;        /* Q15 periodicity value for the current frame */
    Ipp16s             dummy;                 /* dummy variable to ensure struct size is multiple of word size (4 bytes) */
    Ipp32s             smoothPeriodicityQ15;  /* smoothed Q15 value of periodicity */
    Ipp32s             cDSFrameSamps;         /* number of samples in the down-sampled frame */
    Ipp16s*            pBPFrame;              /* buffer to hold the band-pass filtered input data */
    Ipp16s*            pDSFrame;              /* buffer to hold the down-sampled data */
    Ipp32s*            pDelayLine;
    void*              pMemoryBlock;          /* pointer to easily compute the size of the structure */
}
PERStateStruct;

/* Structure for the VAD State Machine (SM) */
typedef struct _SMStruct
{
    Ipp32s    cMinOnsetFrames;          /* minimum duration in frames for speech onset */
    Ipp32s    cMinEnergyHangFrames;     /* minimum energy hang duration in frames of speech end */
    Ipp32s    cMinPerHangFrames;        /* minimum periodicity hang duration in frames of speech end */
    Ipp32s    cUttBegAdjustFrames;      /* number of frames used to adjust the start frame number */
    Ipp32s    cOnsetFrames;             /* number of consecutive onset frames detected */
    Ipp32s    cEnergyHangFrames;        /* number of consecutive hang frames detected based on energy measure */
    Ipp32s    cPerHangFrames;           /* number of consecutive hang frames detected based on periodicity measure*/
    Ipp32s    uttHasStartedFlag;        /* Flag indicating utterance has start has been detected */
    Ipp32s    uttHasEndedFlag;          /* Flag indicating utterance end has been detected */
    Ipp32s    energySpeechIsActiveFlag; /* Flag indicating speech activity has been detected based on energy measure  */
    Ipp32s    perSpeechIsActiveFlag;    /* Flag indicating speech activity has been detected based on periodicity measure */
    Ipp32s    uttBegFrameNum;           /* frame number of start of utterance */
    Ipp32s    uttEndFrameNum;           /* frame number of end of utterance */
    Ipp32s    prevUttEndFrameNum;       /* frame number of the end of previous utterance (including hang time) */
    VADState  state;                    /* state of the VAD state machine */
    void*     pMemoryBlock;             /* pointer to easily compute the size of the structure */
}
SMStruct ;

typedef struct _VADStateStruct
{
    EStateStruct*     pEState;      /* energy-based state structure */
    PERStateStruct*   pPerState;    /* periodicity-based state structure */
    SMStruct*         pSM;          /* state machine structure */
    Ipp32s            frameNum;     /* absolute frame number (starting from 0) of the number of frames read */
    void*             pMemoryBlock; /* pointer to easily compute the size of the structure */
}
VADStateStruct;

/* weighted average using Q15 weight */
#define WEIGHTED_AVG_Q15(val1, val2, wgt, result) \
{                                                 \
  Ipp64s term1,term2;                             \
  term1 = (Ipp64s) val1 * wgt + 1;                \
  term2 = (Ipp64s) val2 * ((1<<15) - wgt) + 1;    \
  result = (Ipp32s)((term1 + term2) >> 15);       \
}

/*
// Prototypes
*/

/* Prototypes of functions related to energy computations (VADenergy.c) */
void E_GetStateSizeBytes(int* pNumStateBytes);
void E_Init(EStateStruct* pEState, int cWinSamps, int frameShiftMsec);
void E_UpdateEnergyState(const Ipp16s* pFrame, int frameNum, EStateStruct* pEState);

/* Prototypes of functions related to periodicity computations (VADper.c) */
void PER_GetStateSizeBytes(int cWinSamps, int* pNumStateBytes);
void PER_Init(PERStateStruct* pPerState, int frameShiftMsec, int cWinSamps, int sampFreqHz);
void PER_UpdatePerState(const Ipp16s* pFrame, int frameNum, int len, PERStateStruct* pPerState);


/* Prototypes of functions related to state machine (VADsm.c) */
void SM_GetStateSizeBytes(int* pNumStateBytes);
void SM_Init(SMStruct* pSMState, int frameShiftMsec);
void SM_UpdateState(VADStateStruct* SM_pState);

#ifdef __cplusplus
}
#endif

#endif // __VADCOMMON_H__

/* EOF */
