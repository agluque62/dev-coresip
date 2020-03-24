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
//  This source file contains the implementation of periodicity-related computations for
//  Voice Activity Detector based upon the Intel Integrated Performance Primitives.
//
*/

/* headers */
#include <ipps.h>
#include <ippsr.h>
#include "vadcommon.h"
#include "vadfiltsr.h"

/* function prototypes */
void PER_BandPassAndDownSample(const Ipp16s* pSrc, Ipp32s srcLen, PERStateStruct* pPerState);
void PER_ComputePeriodicity(const Ipp16s* pInFrame, Ipp32s len, PERStateStruct* pPerState);
void PER_SmoothPeriodicity(PERStateStruct* pPerState, Ipp32s frameNum);

/********************************************************************************
// Name:             PER_GetStateSizeBytes
// Description:      Calculate and return the size in bytes required by the
//                   periodicity internal state structure based on the input parameters
// Input Arguments:
//                   cWinSamps      - size of an input data frame in samples
// Output Arguments:
//                   pNumStateBytes - pointer to output variable that contains the
//                                    calculated state size
// Returns:          None
// Notes:
********************************************************************************/

void PER_GetStateSizeBytes(Ipp32s cWinSamps, Ipp32s* pNumStateBytes)
{
    Ipp32s cTmpBytes;
    Ipp32s cTmpDownSampledWinSamps;

    /* initialize with the size of the static components of the PERStateStruct */
    *pNumStateBytes = sizeof(PERStateStruct);

    /* add the size of pState->pBPFrame - band-pass filtered values */
    cTmpBytes = cWinSamps *sizeof(Ipp16s);
    *pNumStateBytes += cTmpBytes;

    /* add any alignment bytes for word boundary alignment */
    cTmpBytes = cTmpBytes % sizeof(Ipp32s); /* align of word boundary */
    *pNumStateBytes += cTmpBytes;

    /* add the size of pState->pDSFrame - downsampled frame */
    cTmpDownSampledWinSamps = (cWinSamps + PER_DOWNSAMPLE_FACTOR - 1 - PER_DOWNSAMPLE_PHASE) / PER_DOWNSAMPLE_FACTOR;
    cTmpBytes = cTmpDownSampledWinSamps * sizeof(Ipp16s);
    *pNumStateBytes += cTmpBytes;

    /* add any alignment bytes for word boundary alignment */
    cTmpBytes = cTmpBytes % sizeof(Ipp32s); /* align of word boundary */
    *pNumStateBytes += cTmpBytes;

    //cTmpBytes = TAPSLEN * sizeof(Ipp32s);
    cTmpBytes = (NUMBIQUAD*2) * sizeof(Ipp32s);
    *pNumStateBytes += cTmpBytes;

}

/********************************************************************************
// Name:            PER_Init
// Description:     Initialize the Periodicity state structure with initial values. Also,
//                  assign the externally allocated memory to the internal variables
//                  of the PERStateStruct.
//
// Input Arguments:
//                  frameShiftMsec - frame shift for overlapping frames in msec
//                  cWinSamps      - size of an input data frame in samples
//                  sampFreqHz     - sampling frequency of the input data in Hz
// Input/Output Arguments:
//                  pPerState      - pointer to an PERState structure to be initialized
// Returns:         None
********************************************************************************/

void PER_Init(PERStateStruct* pPerState, Ipp32s frameShiftMsec, Ipp32s cWinSamps, Ipp32s sampFreqHz)
{
    Ipp32s   halfFrameShiftMsec;
    Ipp32s   tmpBytes;
    Ipp8s* pMemory;
    Ipp32s   i;

    /* half the frame shift used to round of the computations of variables */
    halfFrameShiftMsec = frameShiftMsec/2;

    /* start memory pointer just after the memory for the PERStateStruct */
    pMemory = (Ipp8s*)(&pPerState->pMemoryBlock + 1);

    /* Initialize periodicity values of state */
    pPerState->cInitPerEstFrames = (PER_INIT_PER_ESTIMATE_MSEC + halfFrameShiftMsec) / frameShiftMsec;
    pPerState->minPeriodSamps = sampFreqHz / (PER_DOWNSAMPLE_FACTOR * PER_MAX_PITCH_FREQ_HZ);
    pPerState->maxPeriodSamps = sampFreqHz / (PER_DOWNSAMPLE_FACTOR * PER_MIN_PITCH_FREQ_HZ);

    pPerState->smoothPeriodicityQ15 = 0;

    /* assign memory to the buffer that holds the bandpass filtered values */
    pPerState->pBPFrame = (Ipp16s *) pMemory;
    tmpBytes = cWinSamps * sizeof(Ipp16s);

    /* align on word boundary */
    tmpBytes += (tmpBytes % sizeof(Ipp32s));
    pMemory += tmpBytes;

    /* assign memory to the buffer that holds the downsampled values */
    pPerState->cDSFrameSamps = (cWinSamps + PER_DOWNSAMPLE_FACTOR - 1 - PER_DOWNSAMPLE_PHASE) / PER_DOWNSAMPLE_FACTOR;
    pPerState->pDSFrame = (Ipp16s *) pMemory;
    tmpBytes = pPerState->cDSFrameSamps * sizeof(Ipp16s);

    /* align on word boundary */
    tmpBytes += (tmpBytes % sizeof(Ipp32s));
    pMemory += tmpBytes;

    /* assign memory to the delayLine for the IIR filter */
    pPerState->pDelayLine = (Ipp32s *) pMemory;
    tmpBytes = (NUMBIQUAD*2) * sizeof(Ipp32s);

    for (i=0; i<(NUMBIQUAD*2); i++)
    {
        pPerState->pDelayLine[i] = 0;
    }
}

/********************************************************************************
// Name:            PER_UpdatePerState
// Description:     Update the periodicity state based on the current input frame.
//
// Input Arguments:
//                  pFrame    - input frame
//                  frameNum  - current frame number used in initializing the noise floor
//                  len       - number of samples in the input frame
// Input/Output Arguments:
//                  pPerState - pointer to an periodicity state structure
//
// Returns:         None
********************************************************************************/
void PER_UpdatePerState(const Ipp16s* pFrame, Ipp32s frameNum, Ipp32s len, PERStateStruct* pPerState)
{
    /* bandpass filter the input data */
    PER_BandPassAndDownSample(pFrame, len, pPerState);

    /* compute periodicity on the bandpass filtered data */
    PER_ComputePeriodicity(pPerState->pDSFrame, pPerState->cDSFrameSamps, pPerState);

    /* smooth the periodicity using history */
    PER_SmoothPeriodicity(pPerState, frameNum);

}

/********************************************************************************
// Name:            PER_BandPassAndDownSample
// Description:     Band-pass filter (70-1000Hz) the input data using Intel(R) IPP data
//
// Input Arguments:
//                  pSrc      - input data
//                  srcLen    - number of samples in the input and output buffers
// Output Arguments:
//                  pPerState - output band pass filtered data
// Returns:         None
********************************************************************************/

void PER_BandPassAndDownSample(const Ipp16s* pSrc, Ipp32s srcLen, PERStateStruct* pPerState)
{
    Ipp32s    phase;
    Ipp32s    dstLen = 0;
    ippsIIR_BiQuadDirect_16s(pSrc, pPerState->pBPFrame, srcLen, pTaps, NUMBIQUAD, pPerState->pDelayLine);

    /* downsample */
    phase = PER_DOWNSAMPLE_PHASE;
    ippsSampleDown_16s(pPerState->pBPFrame, srcLen, pPerState->pDSFrame, &dstLen, PER_DOWNSAMPLE_FACTOR, &phase);
}


/********************************************************************************
// Name:            PER_ComputePeriodicity
// Description:     Compute periodicity using Intel(R) IPP function
//
// Input Arguments:
//                  pInFrame  - input data
//                  len       - number of samples in the input buffer
// Input/Output Arguments:
//                  pPerState - pointer to an periodicity state structure
//
// Returns:         None
********************************************************************************/

void PER_ComputePeriodicity(const Ipp16s* pInFrame, Ipp32s len, PERStateStruct* pPerState)
{
    ippsPeriodicityLSPE_16s(pInFrame,len,&(pPerState->periodicityQ15),&(pPerState->period),pPerState->maxPeriodSamps,pPerState->minPeriodSamps);
}


/********************************************************************************
// Name:            PER_SmoothPeriodicity
// Description:     Smooth the computed periodicity by summing over periodicity history.
//                  The average noise periodicity value is removed before smoothing.
//
// Input Arguments:
//                  frameNum  - current frame number used in initializing the noise floor
// Input/Output Arguments:
//                  pPerState - pointer to an periodicity state structure
//
// Returns:         None
********************************************************************************/

void PER_SmoothPeriodicity(PERStateStruct* pPerState, Ipp32s frameNum)
{
    if (frameNum <= pPerState->cInitPerEstFrames)
    {
        /* Initialize periodicity */
        pPerState->smoothPeriodicityQ15 = pPerState->smoothPeriodicityQ15 * (frameNum-1) + pPerState->periodicityQ15;
        pPerState->smoothPeriodicityQ15 /= frameNum;
    }
    else
    {
        WEIGHTED_AVG_Q15(pPerState->periodicityQ15, pPerState->smoothPeriodicityQ15, PER_ADAPT_ALPHA_Q15,
          pPerState->smoothPeriodicityQ15)
    }
}

/* EOF */
