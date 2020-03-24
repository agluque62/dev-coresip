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
//  This source file is the main file that contains the implementation of a
//  Voice Activity Detector based upon the Intel Integrated Performance Primitives.
//
*/

//-------------   only 16kHz   --------------//

/* headers */
#include <ippdefs.h>
#include <ipps.h>
#include "scratchmem.h"
#include "usc_filter.h"
#include "vadcommon.h"

#define FRAME_SHIFT_MSEC        16                    /* frame shift in msec for overlapping frames */
#define SAMPLE_FREQUENCY 16000

#define VAD_SR_SPEECH_FRAME    256
#define VAD_SR_BITS_PER_SAMPLE 16
#define VAD_SR_NCHANNELS 1

typedef struct {
    Ipp32s vad;
    Ipp32s reserved; // for future extension
    VADStateStruct *pState_;
} SR_Handle_Header;

static USC_Status GetInfoSize_VAD(Ipp32s *pSize);
static USC_Status NumAlloc_VAD(const USC_FilterOption *options, Ipp32s *nbanks);
static USC_Status GetInfo_VAD_SR(USC_Handle handle, USC_FilterInfo *pInfo);

static USC_Status MemAlloc_VAD_SR(const USC_FilterOption *options, USC_MemBank *pBanks);
static USC_Status Init_VAD_SR(const USC_FilterOption *options, const USC_MemBank *pBanks, USC_Handle *handle);
static USC_Status SetDlyLine(USC_Handle handle, Ipp8s *pDlyLine);
static USC_Status VAD_SR(USC_Handle handle, USC_PCMStream *in, USC_PCMStream *out, USC_FrameType *pVADDecision);
static void       VAD_ProcessEndOfInput(VADStateStruct *pState_, USC_FrameType *pDecision_State, Ipp32s *pDecisionFrame);

/* global usc vector table */
USCFUN USC_Filter_Fxns USC_SR_VAD_Fxns=
{
    {
      USC_FilterVAD,
      (GetInfoSize_func) GetInfoSize_VAD,
      (GetInfo_func)     GetInfo_VAD_SR,
      (NumAlloc_func)    NumAlloc_VAD,
      (MemAlloc_func)    MemAlloc_VAD_SR,
      (Init_func)        Init_VAD_SR,
      (Reinit_func)      USC_NoError,
      (Control_func)     USC_NoError
    },
    SetDlyLine,
    VAD_SR
};

static __ALIGN32 CONST USC_PCMType pcmTypesTbl_VAD_SR[1]={
   {SAMPLE_FREQUENCY,VAD_SR_BITS_PER_SAMPLE}
};

static USC_Status SetDlyLine(USC_Handle handle, Ipp8s *pDlyLine)
{
   if(pDlyLine==NULL) return USC_BadDataPointer;
   if(handle==NULL) return USC_InvalidHandler;

   return USC_NoOperation;
}

USC_Status MemAlloc_VAD_SR(const USC_FilterOption *options, USC_MemBank *pBanks)
{
    Ipp32s cWinSamps;
    Ipp32s pNumStateBytes;
    Ipp32s cTmpStateBytes; /* size of the intermediate states in bytes */
    USC_CHECK_PTR(pBanks);
    USC_CHECK_PTR(options);

    cWinSamps = options->maxframesize;

   /* initialize with the size of the static components of the VADStateStruct */
    pNumStateBytes = sizeof(VADStateStruct);

    /* add the size of the energy state structure */
    E_GetStateSizeBytes(&cTmpStateBytes);
    pNumStateBytes += cTmpStateBytes;

    /* add the size of the periodicity state structure */
    PER_GetStateSizeBytes(cWinSamps, &cTmpStateBytes);
    pNumStateBytes += cTmpStateBytes;

    /* add the size of the state-machine structure */
    SM_GetStateSizeBytes(&cTmpStateBytes);
    pNumStateBytes += cTmpStateBytes;
    pBanks[0].pMem = NULL;
    pBanks[0].nbytes = pNumStateBytes + sizeof(SR_Handle_Header);
    pBanks[1].pMem = NULL;
    pBanks[1].nbytes = 5120+40;
    return USC_NoError;
}

USC_Status Init_VAD_SR(const USC_FilterOption *options, const USC_MemBank *pBanks, USC_Handle *handle)
{
    Ipp8s *pMemory;      /* pointer to current memory block to be assigned */
    Ipp32s cStateBytes;  /* size of the intermediate states in bytes */
   Ipp32s cWinSamps, sampFreqHz, frameShiftMsec;
   SR_Handle_Header *sr_header;

   USC_CHECK_PTR(options);
   USC_CHECK_PTR(pBanks);
   USC_CHECK_HANDLE(handle);
   USC_CHECK_PTR(pBanks[0].pMem);
   USC_BADARG(pBanks[0].nbytes<=0, USC_NotInitialized);
   USC_CHECK_PTR(pBanks[1].pMem);
   USC_BADARG(pBanks[1].nbytes<=0, USC_NotInitialized);

   USC_BADARG(options->modes.vad > 1, USC_UnsupportedVADType);
   USC_BADARG(options->modes.vad < 0, USC_UnsupportedVADType);

   USC_BADARG(options->pcmType.sample_frequency < 1, USC_UnsupportedPCMType);
   USC_BADARG(options->pcmType.bitPerSample < 1, USC_UnsupportedPCMType);

   cWinSamps = options->maxframesize/2;
   sampFreqHz = 16000;
   frameShiftMsec = FRAME_SHIFT_MSEC;

    *handle = (USC_Handle*)pBanks[0].pMem;
    sr_header = (SR_Handle_Header*)*handle;
   USC_CHECK_PTR(sr_header);
    sr_header->pState_ = (VADStateStruct*)((Ipp8s*)*handle + sizeof(SR_Handle_Header));

    /* start memory pointer just after the memory for the VADStateStruct */
    pMemory = (Ipp8s*)(&sr_header->pState_->pMemoryBlock + 1);

    /* assign memory and initialize the energy state structure */
    sr_header->pState_->pEState = (EStateStruct *) pMemory;
    E_Init(sr_header->pState_->pEState, cWinSamps, frameShiftMsec);
    E_GetStateSizeBytes(&cStateBytes);
    pMemory += cStateBytes;

    /* assign memory and initialize the periodicity state structure */
    sr_header->pState_->pPerState = (PERStateStruct *) pMemory;
    PER_Init(sr_header->pState_->pPerState, frameShiftMsec, cWinSamps, sampFreqHz);
    PER_GetStateSizeBytes(cWinSamps, &cStateBytes);
    pMemory += cStateBytes;

    /* assign memory and initialize the state-machine structure */
    sr_header->pState_->pSM = (SMStruct *) pMemory;
    SM_Init(sr_header->pState_->pSM, frameShiftMsec);
    SM_GetStateSizeBytes(&cStateBytes);
    pMemory += cStateBytes;

    /* initialize the frame count */
    sr_header->pState_->frameNum = 0;
    return USC_NoError;
}

USC_Status VAD_SR(USC_Handle handle, USC_PCMStream *in, USC_PCMStream *out, USC_FrameType *pVADDecision)
{
   USC_FrameType  oldState;
   Ipp32s pVD;
   SR_Handle_Header *sr_header;
   Ipp32s len;

   USC_CHECK_PTR(in);
   USC_CHECK_PTR(out);
   USC_CHECK_PTR(pVADDecision);
   USC_CHECK_HANDLE(handle);
   USC_BADARG(in->nbytes<VAD_SR_SPEECH_FRAME*sizeof(Ipp16s), USC_NoOperation);
   USC_BADARG(in->pcmType.bitPerSample!=VAD_SR_BITS_PER_SAMPLE, USC_UnsupportedPCMType);
   USC_BADARG(in->pcmType.sample_frequency!=SAMPLE_FREQUENCY, USC_UnsupportedPCMType);

   USC_BADARG(in->pBuffer == NULL, USC_NoOperation);
   USC_BADARG(out->pBuffer == NULL, USC_NoOperation);

   len = in->nbytes / 2;

   sr_header = (SR_Handle_Header*)handle;
   sr_header->pState_ = (VADStateStruct*)((Ipp8s*)handle + sizeof(SR_Handle_Header));

   oldState = *pVADDecision;
   if (in == NULL) {
      VAD_ProcessEndOfInput(sr_header->pState_, pVADDecision, &pVD);
   }
   else {
      const Ipp16s* pIn_Frame;
      pIn_Frame = (Ipp16s*)in->pBuffer;

      /* update frame number count */
      sr_header->pState_->frameNum++;

      /* Energy-Based state update */
      E_UpdateEnergyState(pIn_Frame, sr_header->pState_->frameNum, sr_header->pState_->pEState);

      /* Periodicity-Based state update */
      PER_UpdatePerState(pIn_Frame, sr_header->pState_->frameNum, len, sr_header->pState_->pPerState);

      /* State Machine update */
      SM_UpdateState(sr_header->pState_);

      ippsCopy_8u((Ipp8u*)in->pBuffer,(Ipp8u*)out->pBuffer, VAD_SR_SPEECH_FRAME*sizeof(Ipp16s));
      /* Update the output decision variables based on the VAD internal state */
      if (sr_header->pState_->pSM->uttHasStartedFlag)
      {
         /* if utterance start detected */
         *pVADDecision = ACTIVE;

         /* reset the start flag for the next utterance */
         sr_header->pState_->pSM->uttHasStartedFlag=0;
      }
      else if (sr_header->pState_->pSM->uttHasEndedFlag)
      {
         /* if utterance end detected */
         *pVADDecision = INACTIVE;
         ippsZero_8u((Ipp8u*)out->pBuffer, VAD_SR_SPEECH_FRAME*sizeof(Ipp16s));
         /* reset the end flag for the next utterance */
         sr_header->pState_->pSM->uttHasEndedFlag = 0;
      }
      else
      {
         if (oldState == ACTIVE){
            *pVADDecision = ACTIVE;
         }
         else{
            /* if neither start or end of utterance was detected */
            *pVADDecision = NODECISION;
            ippsZero_8u((Ipp8u*)out->pBuffer, VAD_SR_SPEECH_FRAME*sizeof(Ipp16s));
         }
      }
   }
   out->nbytes = VAD_SR_SPEECH_FRAME*sizeof(Ipp16s);

   return USC_NoError;
}

void VAD_ProcessEndOfInput(VADStateStruct *pState_, USC_FrameType *pDecision_State, Ipp32s *pDecisionFrame)
{
   /* Update the output decision variables based on the VAD internal state */
   if (pState_->pSM->uttHasEndedFlag)
   {
        /* if utterance end detected */
        *pDecision_State = INACTIVE;
        *pDecisionFrame = pState_->pSM->uttEndFrameNum;

        /* reset the end flag for the next utterance */
        pState_->pSM->uttHasEndedFlag = 0;
    }
    else
    {
        /* declare end of stream to flush the complete output buffer */
        *pDecision_State = END_OF_STREAM;
        *pDecisionFrame = -1;
    }
}

/* EOF */

static USC_Status NumAlloc_VAD(const USC_FilterOption *options, Ipp32s *nbanks)
{
   USC_CHECK_PTR(options);
   USC_CHECK_PTR(nbanks);
   *nbanks = 2;
   return USC_NoError;
}

static USC_Status GetInfoSize_VAD(Ipp32s *pSize)
{
   USC_CHECK_PTR(pSize);
   *pSize = sizeof(USC_FilterInfo);
   return USC_NoError;
}

static USC_Status GetInfo_VAD_SR(USC_Handle handle, USC_FilterInfo *pInfo)
{
    SR_Handle_Header *sr_header;
    USC_CHECK_PTR(pInfo);

    pInfo->name = "IPP_VAD";
    pInfo->params->maxframesize = VAD_SR_SPEECH_FRAME*sizeof(Ipp16s);
    pInfo->params->minframesize = VAD_SR_SPEECH_FRAME*sizeof(Ipp16s);
    pInfo->params->framesize = VAD_SR_SPEECH_FRAME*sizeof(Ipp16s);

    if (handle == NULL) {
       pInfo->params->modes.vad = 1;
    } else {
       sr_header = (SR_Handle_Header *)handle;
       pInfo->params->modes.vad = sr_header->vad;
    }
    pInfo->params->pcmType.sample_frequency = pcmTypesTbl_VAD_SR[0].sample_frequency;
    pInfo->params->pcmType.bitPerSample = pcmTypesTbl_VAD_SR[0].bitPerSample;
    pInfo->params->pcmType.nChannels = 1;
    pInfo->nOptions = 1;

   return USC_NoError;
}
