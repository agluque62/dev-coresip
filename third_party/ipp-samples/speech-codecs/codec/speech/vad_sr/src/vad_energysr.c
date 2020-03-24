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
//  This source file contains the implementation of energy-related computations for
//  Voice Activity Detector based upon the Intel(R) Integrated Performance Primitives.
//
*/

/* headers */
#include <ipps.h>
#include "vadcommon.h"

#define   Q15    15  /* N value of a Q15 (QM.N format) number */

/* Function prototypes */
void E_ComputeEnergyDB(const Ipp16s* pFrame, EStateStruct* pEState);
void E_ComputeNoiseEnergyThreshDB(EStateStruct* pEState, Ipp32s frameNum);

/********************************************************************************
// Name:             E_GetStateSizeBytes
// Description:      Calculate and return the size in bytes required by the Energy
//                   state structure.
// Input Arguments:
//                   None
// Output Arguments:
//                   pNumStateBytes - pointer to output variable containing the
//                                    calculated  state size
// Returns:          None
// Notes:
********************************************************************************/
void E_GetStateSizeBytes(Ipp32s* pNumStateBytes)
{
    /* initialize with the size of the static components of the EStateStruct */
    *pNumStateBytes = sizeof(EStateStruct);
}

/********************************************************************************
// Name:            E_Init
// Description:     Initialize the Energy state structure with initial values. Also,
//                  assign the externally allocated memory to the internal variables
//                  of the EStateStruct.
//
// Input Arguments:
//                  cWinSamps      - size of an input data frame in samples
//                  frameShiftMsec - frame shift for overlapping frames in msec
// Input/Output Arguments:
//                  pEState        - pointer to an EState structure
// Returns:         None
********************************************************************************/
void E_Init(EStateStruct* pEState, Ipp32s cWinSamps, Ipp32s frameShiftMsec)
{
    Ipp32s    halfFrameShiftMsec;
    Ipp32s     i;

    /* number of samples in a frame */
    pEState->cFrameSamples = cWinSamps;

    /* find number of right shifts for terms in energy summation */
    if (cWinSamps > 0)
    {
      pEState->cScaleFactor = 0;
      i = cWinSamps;
      while(i>0)
      {
        pEState->cScaleFactor++;
        i >>= 1;
      }
    }

    /* half the frame shift used to round of the computations of variables */
    halfFrameShiftMsec = frameShiftMsec/2;

    /* Initialize variables */
    pEState->cInitNoiseEstFrames = (E_INIT_NOISE_ESTIMATE_MSEC + halfFrameShiftMsec) / frameShiftMsec;

    /* Initial value to compute running average in order to initialize noise floor */
    pEState->noiseFloorDB = E_MIN_ENERGY_DB;
}

/********************************************************************************
// Name:            E_UpdateEnergyState
// Description:     Update the energy state based on the current input frame.
//
// Input Arguments:
//                  pFrame    - input frame
//                  frameNum  - current frame number used in initializing the noise floor
// Input/Output Arguments:
//                  pEState   - pointer to an EState structure
//
// Returns:         None
********************************************************************************/
void E_UpdateEnergyState(const Ipp16s* pFrame, Ipp32s frameNum, EStateStruct* pEState)
{
    /* compute frame energy in DB */
    E_ComputeEnergyDB(pFrame, pEState);

    /* compute threshold for noise for the current frame */
    E_ComputeNoiseEnergyThreshDB(pEState, frameNum);
}

/********************************************************************************
// Name:            E_ComputeEnergyDB
// Description:     Compute the energy of the input frame in DB. The energy is computed
//                  as the variance of the samples in the input frame. The variance is
//                  subsequently converted into DB (10log10).
//
// Input Arguments:
//                  pFrame         - input frame
// Input/Output Arguments:
//                  pEState      - pointer to an EState structure
//
// Returns:         None
********************************************************************************/
void E_ComputeEnergyDB(const Ipp16s* pFrame, EStateStruct* pEState)
{
    Ipp32s  sumSqr;    /* sum square of the input samples */
    Ipp32s     i;

    sumSqr = 0;

    /* compute sum-square and sum of the input samples */
    for (i=0; i<pEState->cFrameSamples; i++)
    {
        sumSqr  += (pFrame[i]*pFrame[i]) >> pEState->cScaleFactor;
    }

    /* convert to DB using Intel(R) IPP call */
    if (sumSqr > 0)
    {
        ipps10Log10_32s_Sfs(&sumSqr, &(pEState->energyDB), 1, -Q15);
    }
    else
    {
        pEState->energyDB = E_MIN_ENERGY_DB;
    }
}

/********************************************************************************
// Name:            E_ComputeNoiseEnergyThreshDB
// Description:     Compute the noise energy threshold of the input frame in DB.
//                  The noise threshold is computed as the sum of a noise floor and
//                  a correction term.

//                  The noise floor is initialized as the average
//                  energy over the initial E_INIT_NOISE_ESTIMATE_MSEC of input data.
//                  The noise floor is then updated in hypothesized non-speech regions
//                  using either a slow or fast adaptation factor.
//
// Input Arguments:
//                  frameNum  - current frame number used in initializing the noise floor
// Input/Output Arguments:
//                  pEState - pointer to an EState structure
//
// Returns:         None
********************************************************************************/
void E_ComputeNoiseEnergyThreshDB(EStateStruct* pEState, Ipp32s frameNum)
{
    if (frameNum <= pEState->cInitNoiseEstFrames)
    {
        /* Initialize noise floor */
        pEState->noiseFloorDB = pEState->noiseFloorDB * (frameNum-1) + pEState->energyDB;
        pEState->noiseFloorDB /= frameNum;
    }
    else
    {
        if (pEState->energyDB > pEState->noiseThreshDB)
        {
            WEIGHTED_AVG_Q15(pEState->energyDB, pEState->noiseFloorDB, E_SLOW_ADAPT_ALPHA_Q15, pEState->noiseFloorDB)
        }
        else
        {
            WEIGHTED_AVG_Q15(pEState->energyDB, pEState->noiseFloorDB, E_FAST_ADAPT_ALPHA_Q15, pEState->noiseFloorDB)
        }
    }

    pEState->noiseThreshDB = pEState->noiseFloorDB + E_THRESH_OFFSET_DB_Q15;
}

/* EOF */
