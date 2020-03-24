/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2002-2008 Intel Corporation. All Rights Reserved.
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
//  This source file contains the implementation of state machine (SM)
//  for the Voice Activity Detector.
//
*/

/* headers */
//#include "VADsr_usc.h"
#include "vadcommon.h"

/* function prototypes */
void SM_UpdateSMParams(VADStateStruct* pState);
void SM_UpdateSMState(SMStruct* pState, Ipp32s frameNum);

/********************************************************************************
// Name:             SM_GetStateSizeBytes
// Description:      Calculate and return the size in bytes required by the State
//                   Machine (SM) structure.
// Output Arguments:
//                   pNumStateBytes  - return variable holding the calculated state size
//
// Returns:          None
// Notes:
********************************************************************************/
void SM_GetStateSizeBytes(Ipp32s* pNumStateBytes)
{
    /* size of the SMStruct */
    *pNumStateBytes = sizeof(SMStruct);
}

/********************************************************************************
// Name:            SM_Init
// Description:     Initialize the state machine structure with initial values.
//
// Input Arguments:
//                  frameShiftMsec - frame shift for overlapping frames in msec
// Input/Output Arguments:
//                  pSMState       - pointer to an SM structure to be initialized
//
// Returns:         None
********************************************************************************/
void SM_Init(SMStruct* pSMState, Ipp32s frameShiftMsec)
{
    /* half the frame shift used to round of the computations of variables */
    Ipp32s halfFrameShiftMsec = frameShiftMsec/2;

    /* Initialize variables */
    pSMState->cMinOnsetFrames = (ONSET_THRESHOLD_MSEC + halfFrameShiftMsec) / frameShiftMsec;
    pSMState->cMinEnergyHangFrames  = (ENERGY_HANG_THRESH_MSEC + halfFrameShiftMsec) / frameShiftMsec;
    pSMState->cMinPerHangFrames  = (PER_HANG_THRESH_MSEC + halfFrameShiftMsec) / frameShiftMsec;
    pSMState->cUttBegAdjustFrames = (UTT_BEG_ADJUSTMENT_MSEC + halfFrameShiftMsec) / frameShiftMsec;
    pSMState->cOnsetFrames = 0;
    pSMState->cEnergyHangFrames = 0;
    pSMState->cPerHangFrames = 0;
    pSMState->uttHasStartedFlag = 0;
    pSMState->uttHasEndedFlag = 0;
    pSMState->state = SILENCE;
    pSMState->uttEndFrameNum = 0;
    pSMState->uttBegFrameNum = 0;
    pSMState->prevUttEndFrameNum=0;

}

/********************************************************************************
// Name:            SM_UpdateSMState
// Description:     Update the state machine based on energy and periodicity
//
// Input/Output Arguments:
//                  pState - pointer to the VAD state structure
//
// Returns:         None
********************************************************************************/
void SM_UpdateState(VADStateStruct* pState)
{
    /* update state machine parameters based on the energy and periodicity measures */
    SM_UpdateSMParams(pState);

    /* determine state of the VAD */
    SM_UpdateSMState(pState->pSM, pState->frameNum);

    if (pState->pSM->uttHasStartedFlag)
    {
        /* adjust the start frame number by a fixed number of frames */
        pState->pSM->uttBegFrameNum -= pState->pSM->cUttBegAdjustFrames;

        /* prevent over-adjustment */
        if (pState->pSM->uttBegFrameNum <= pState->pSM->prevUttEndFrameNum)
        {
          pState->pSM->uttBegFrameNum = pState->pSM->prevUttEndFrameNum + 1;
        }
    }
    else if (pState->pSM->uttHasEndedFlag)
    {
        /* save the end-of-utterance frame number to use in calculation of history for start of next utterance */
        pState->pSM->prevUttEndFrameNum = pState->frameNum;
    }
}

/********************************************************************************
// Name:            SM_UpdateStateParams
// Description:     Update the energy and periodicity related flags based on the
//                  their values with respect to their thresholds.
//
// Input/Output Arguments:
//                  pState       - pointer to the VADState structure.
//
// Returns:         None
********************************************************************************/
void SM_UpdateSMParams(VADStateStruct* pState)
{
    /*
    // If the current frame energy exceeds the noise threshold, this indicates
    // that speech is present in the current frame.
    */
    if (pState->pEState->energyDB > pState->pEState->noiseThreshDB)
    {
        pState->pSM->energySpeechIsActiveFlag = 1;
    }
    else
    {
        pState->pSM->energySpeechIsActiveFlag = 0;
    }

    /*
    // If the current frame periodicity exceeds the threshold, this indicates
    // that (voiced) speech is present in the current frame.
    */
    if (pState->pPerState->smoothPeriodicityQ15 > PER_SPEECH_THRESHOLD_Q15)
    {
        pState->pSM->perSpeechIsActiveFlag = 1;
    }
    else
    {
        pState->pSM->perSpeechIsActiveFlag = 0;
    }

}

/********************************************************************************
// Name:            SM_UpdateState
// Description:     Update the state of the VAD state machine based on onset and
//                  hang times. The VAD state machine can be in one of the following states
//                  1) SILENCE - No speech is present.
//                               Allowed transitions -
//                               SILENCE -> SILENCE : at least one of the energy/periodicity
//                                                    flags indicates speech inactivity.
//                               SILENCE -> ONSET   : both energy/periodicity flag indicate
//                                                    speech activity
//
//                  2) ONSET   - Start of an utterance may have been detected.
//                               Allowed transitions -
//                               ONSET -> SPEECH    : both the energy/periodicity flags indicate
//                                                    speech activity in consecutive ONSET_THRESHOLD_MSEC
//                                                    frames. This transition sets the utterance start flag.
//                               ONSET -> SILENCE   : both the energy/periodicity flags are false (do not
//                                                    indicate speech activity) in consecutive
//                                                    ONSET_THRESHOLD_MSEC frames
//                               ONSET -> ONSET     : both energy/periodicity flag indicate
//                                                    speech activity but ONSET_THRESHOLD_MSEC
//                                                    has not been reached

//                  3) SPEECH  - Speech is present.
//                               Allowed transitions -
//                               SPEECH -> HANG     : at least one of energy/periodicity
//                                                    flags do not indicate speech activity
//                               SPEECH -> SPEECH   : both energy/periodicity flag indicate
//                                                    speech activity
//
//                  4) HANG    - End of an utterance may have been detected.
//                               Allowed transitions -
//                               HANG -> SPEECH     : if both energy/periodicity flags indicate
//                                                    speech activity before hang times
//                                                    ENERGY_HANG_THRESH_MSEC or PER_HANG_THRESH_MSEC
//                                                    are completed
//                               HANG -> HANG       : at least one of energy/periodicity
//                                                    flags do not indicate speech activity in consecutive
//                                                    frames but hang time ENERGY_HANG_THRESH_MSEC or
//                                                    PER_HANG_THRESH_MSEC are not yet completed
//                               HANG -> SILENCE    : at least one of energy/periodicity flags do not indicate
//                                                    speech activity in consecutive frames and hang times
//                                                    ENERGY_HANG_THRESH_MSEC or PER_HANG_THRESH_MSEC are
//                                                    is completed. This transition sets the utterance end flag.
// Input Arguments:
//                  frameNum     - current frame number used for setting the start/end frame number.
// Input/Output ARguments:
//                  pState       - pointer to the SM structure.
//
// Returns:         None
********************************************************************************/
void SM_UpdateSMState(SMStruct* pSM, Ipp32s frameNum)
{

    switch (pSM->state)
    {
        case SILENCE:
            if (pSM->energySpeechIsActiveFlag && pSM->perSpeechIsActiveFlag )
            {
                /* speech onset detected. Transition SILENCE -> ONSET */
                pSM->state = ONSET;
                pSM->cOnsetFrames = 1;
            }
            break;

        case ONSET:
            if (pSM->energySpeechIsActiveFlag && pSM->perSpeechIsActiveFlag )
            {
                /* update number of onset frames */
                (pSM->cOnsetFrames)++;

                if (pSM->cOnsetFrames >= pSM->cMinOnsetFrames)
                {
                    /* utterance start detected. Transition ONSET -> SPEECH */
                    pSM->uttHasStartedFlag = 1;
                    pSM->uttBegFrameNum = frameNum - pSM->cMinOnsetFrames;
                    pSM->state = SPEECH;
                }
            }
            else
            {
                /* False onset. Reset onset frames. Transition ONSET -> SILENCE */
                pSM->state = SILENCE;
                pSM->cOnsetFrames = 0;
            }
            break;

        case SPEECH:
            if (!(pSM->energySpeechIsActiveFlag))
            {
                /* utterance end may have started. Transition SPEECH -> HANG */
                pSM->cEnergyHangFrames=1;
                pSM->state = HANG;
            }

            if (!(pSM->perSpeechIsActiveFlag))
            {
                /* utterance end may have started. Transition SPEECH -> HANG */
                pSM->cPerHangFrames=1;
                pSM->state = HANG;
            }

            break;

        case HANG:
            if (!(pSM->energySpeechIsActiveFlag))
            {
                /* update number of consecutive hang frames based on energy measure */
                (pSM->cEnergyHangFrames)++;
            }
            else
            {
                /* reset to zero since consecutive frames not inactive */
                pSM->cEnergyHangFrames = 0;
            }
            if (!(pSM->perSpeechIsActiveFlag))
            {
                /* update number of consecutive hang frames based on energy measure */
                (pSM->cPerHangFrames)++;
            }
            else
            {
                /* reset to zero since consecutive frames not inactive */
                pSM->cPerHangFrames = 0;
            }
            if ( (pSM->cEnergyHangFrames >= pSM->cMinEnergyHangFrames)
              || (pSM->cPerHangFrames >= pSM->cMinPerHangFrames) )
            {
                /* speech end detected. Transition HANG -> SILENCE */
                pSM->uttEndFrameNum = frameNum - 1;
                pSM->uttHasEndedFlag = 1;
                pSM->cEnergyHangFrames = 0;
                pSM->cPerHangFrames = 0;
                pSM->state = SILENCE;
            }
            else if ((0 == pSM->cEnergyHangFrames)
              && (0 == pSM->cPerHangFrames))
            {
                /*
                // false HANG since both energy/periodicity flags indicate speech activity.
                // Transition HANG -> SPEECH
                */
                pSM->state = SPEECH;
            }
            break;

        default:
            break;
    }
}


/* EOF */
