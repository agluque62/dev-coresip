/*/////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2007-2008 Intel Corporation. All Rights Reserved.
//
//     Intel(R) Integrated Performance Primitives
//     USC - Unified Speech Codec interface library
//
// By downloading and installing USC codec, you hereby agree that the
// accompanying Materials are being provided to you under the terms and
// conditions of the End User License Agreement for the Intel(R) Integrated
// Performance Primitives product previously accepted by you. Please refer
// to the file ippEULA.rtf or ippEULA.txt located in the root directory of your Intel(R) IPP
// product installation for more information.
//
// A speech coding standards promoted by ITU, ETSI, 3GPP and other
// organizations. Implementations of these standards, or the standard enabled
// platforms may require licenses from various entities, including
// Intel Corporation.
//
//
// Purpose: Noise reduction: USC functions.
//
*/
#if defined( _WIN32_WCE)
#pragma warning( disable : 4505 )
#endif

#include "usc_filter.h"
#include <ipps.h>
#include "nr.h"

static USC_Status GetInfoSize(Ipp32s *pSize);
static USC_Status GetInfo(USC_Handle handle, USC_FilterInfo *pInfo);
static USC_Status NumAlloc(const USC_FilterOption *options, Ipp32s *nbanks);
static USC_Status MemAlloc(const USC_FilterOption *options, USC_MemBank *pBanks);
static USC_Status Init(const USC_FilterOption *options, const USC_MemBank *pBanks, USC_Handle *handle);
static USC_Status Reinit(const USC_FilterModes *modes, USC_Handle handle );
static USC_Status Control(const USC_FilterModes *modes, USC_Handle handle );
static USC_Status SetDlyLine(USC_Handle handle, Ipp8s *pDlyLine);
static USC_Status Filter(USC_Handle handle, USC_PCMStream *pIn, USC_PCMStream *pOut, USC_FrameType *pDecision);

#define MIN_NR_FRAME_SIZE      128
#define PREF_NR_FRAME_SIZE     128
#define MAX_NR_FRAME_SIZE      160

#define NR_TWO_FRAMES_SIZE     (nr_header->framesize<<1)

#define IPPSR_NR_ALG_SAMPLES_NUM     nr_header->framesize
#define MAX_IPPSR_NR_ALG_SAMPLES_NUM MAX_NR_FRAME_SIZE

#define EXP_NUM_FRAMES_INIT 1024 /* 2^n where n is the number of frames to use for noise initialization */

typedef struct {
    Ipp32u cExpInitFramesLeft;
    Ipp8u tmpBuffer[MAX_NR_FRAME_SIZE<<1];
    Ipp32s sampleFrequence;
    Ipp32s framesize;
    Ipp32s reserved1;
} NR_Handle_Header;

/* global usc vector table */
USCFUN USC_Filter_Fxns USC_NR_Fxns=
{
    {
        USC_Filter,
        (GetInfoSize_func) GetInfoSize,
        (GetInfo_func)     GetInfo,
        (NumAlloc_func)    NumAlloc,
        (MemAlloc_func)    MemAlloc,
        (Init_func)        Init,
        (Reinit_func)      Reinit,
        (Control_func)     Control
    },
    SetDlyLine,
    Filter
};

static USC_Status GetInfoSize(Ipp32s *pSize)
{
    *pSize = sizeof(USC_FilterInfo);
    return USC_NoError;
}

static USC_Status GetInfo(USC_Handle handle, USC_FilterInfo *pInfo)
{
    pInfo->name = "IPP_NR_EPHM";

    pInfo->nOptions = 1;
    pInfo->params[0].maxframesize = MAX_NR_FRAME_SIZE;
    pInfo->params[0].minframesize = MIN_NR_FRAME_SIZE;
    pInfo->params[0].framesize = PREF_NR_FRAME_SIZE;
    pInfo->params[0].pcmType.bitPerSample = 16;
    pInfo->params[0].pcmType.sample_frequency = 8000;
    pInfo->params[0].pcmType.nChannels = 1;
    pInfo->params[0].modes.vad = 0;
    pInfo->params[0].modes.reserved2 = 0;
    pInfo->params[0].modes.reserved3 = 0;
    pInfo->params[0].modes.reserved4 = 0;

    handle = handle;

    return USC_NoError;
}

static USC_Status NumAlloc(const USC_FilterOption *options, Ipp32s *nbanks)
{
   if(options==NULL) return USC_BadDataPointer;
   if(nbanks==NULL) return USC_BadDataPointer;
   *nbanks = 2;
   return USC_NoError;
}

static USC_Status MemAlloc(const USC_FilterOption *options, USC_MemBank *pBanks)
{
   Ipp32s NRAlgSize;
   Ipp32s NRLocalAlgMemSize;

   if(options==NULL) return USC_BadDataPointer;
   if(pBanks==NULL) return USC_BadDataPointer;

   pBanks[0].pMem = NULL;
   pBanks[0].align = 32;
   pBanks[0].memType = USC_OBJECT;
   pBanks[0].memSpaceType = USC_NORMAL;

   NR_GetMemoryReq(MAX_IPPSR_NR_ALG_SAMPLES_NUM,&NRAlgSize);

   pBanks[0].nbytes = NRAlgSize + sizeof(NR_Handle_Header);

   pBanks[1].pMem = NULL;
   pBanks[1].align = 32;
   pBanks[1].memType = USC_BUFFER;
   pBanks[1].memSpaceType = USC_NORMAL;

   if(NR_GetLocalMemoryReq(&NRLocalAlgMemSize) < 0) {
      pBanks[1].nbytes = 0;
      return USC_NoOperation;
   }
   pBanks[1].nbytes = NRLocalAlgMemSize;
   return USC_NoError;
}

static USC_Status Init(const USC_FilterOption *options, const USC_MemBank *pBanks, USC_Handle *handle)
{
   NR_Handle_Header *nr_header;
   NRstate* pNRState;

   if(options==NULL) return USC_BadDataPointer;
   if(pBanks==NULL) return USC_BadDataPointer;
   if(pBanks[0].pMem==NULL) return USC_NotInitialized;
   if(pBanks[0].nbytes<=0) return USC_NotInitialized;
   if(pBanks[1].pMem==NULL) return USC_NotInitialized;
   if(pBanks[1].nbytes<=0) return USC_NotInitialized;
   if(handle==NULL) return USC_InvalidHandler;

   *handle = (USC_Handle*)pBanks[0].pMem;
   nr_header = (NR_Handle_Header*)*handle;

   nr_header->cExpInitFramesLeft = EXP_NUM_FRAMES_INIT;
   nr_header->framesize = options->framesize;

   /* Init NR */
   pNRState = (NRstate*)((Ipp8s*)nr_header + sizeof(NR_Handle_Header));
   NR_Init(&pNRState, pNRState, IPPSR_NR_ALG_SAMPLES_NUM, options->pcmType.sample_frequency);

   NR_InitBuff(pNRState, pBanks[1].pMem);

   ippsZero_8u(nr_header->tmpBuffer,NR_TWO_FRAMES_SIZE);

   nr_header->sampleFrequence = options->pcmType.sample_frequency;
   nr_header->reserved1 = 0;

   return USC_NoError;
}

static USC_Status Reinit(const USC_FilterModes *modes, USC_Handle handle )
{
   NR_Handle_Header *nr_header;
   NRstate* pNRState;

   if(modes==NULL) return USC_BadDataPointer;
   if(handle==NULL) return USC_InvalidHandler;

   nr_header = (NR_Handle_Header*)handle;

   pNRState = (NRstate*)((Ipp8s*)nr_header + sizeof(NR_Handle_Header));
   nr_header->cExpInitFramesLeft = EXP_NUM_FRAMES_INIT;

   NR_Init(&pNRState, pNRState, nr_header->framesize, nr_header->sampleFrequence);

   NR_ReInitBuff(pNRState);

   ippsZero_8u(nr_header->tmpBuffer,NR_TWO_FRAMES_SIZE);

   return USC_NoError;
}

static USC_Status Control(const USC_FilterModes *modes, USC_Handle handle )
{
   if(modes==NULL) return USC_BadDataPointer;
   if(handle==NULL) return USC_InvalidHandler;
   return USC_NoError;
}

static USC_Status SetDlyLine(USC_Handle handle, Ipp8s *pDlyLine)
{
   NR_Handle_Header *nr_header;

   if(pDlyLine==NULL) return USC_BadDataPointer;
   if(handle==NULL) return USC_InvalidHandler;

   nr_header = (NR_Handle_Header*)handle;
   ippsCopy_8u((Ipp8u*)pDlyLine,(Ipp8u*)((Ipp8u*)nr_header->tmpBuffer + nr_header->framesize),nr_header->framesize);
   return USC_NoError;
}
static USC_Status Filter(USC_Handle handle, USC_PCMStream *pIn, USC_PCMStream *pOut, USC_FrameType *pDecision)
{
   NR_Handle_Header *nr_header;
   NRstate* pNRState;

   if(pIn==NULL) return USC_BadDataPointer;
   if(pOut==NULL) return USC_BadDataPointer;
   if(pIn->pBuffer==NULL) return USC_BadDataPointer;
   if(pOut->pBuffer==NULL) return USC_BadDataPointer;
   if(handle==NULL) return USC_InvalidHandler;

   nr_header = (NR_Handle_Header*)handle;
   pNRState = (NRstate*)((Ipp8s*)nr_header + sizeof(NR_Handle_Header));

   if(pIn->nbytes < nr_header->framesize) {
      pIn->nbytes = 0;
      pOut->nbytes = 0;
      pOut->pBuffer=NULL;
      if(pDecision) {
         *pDecision = NODECISION;
      }
      return USC_NoError;
   }

   if(pOut->nbytes < nr_header->framesize) {
      pIn->nbytes = 0;
      pOut->nbytes = 0;
      pOut->pBuffer=NULL;
      if(pDecision) {
         *pDecision = NODECISION;
      }
      return USC_NoError;
   }

   ippsCopy_8u((Ipp8u*)((Ipp8u*)nr_header->tmpBuffer + nr_header->framesize),nr_header->tmpBuffer,nr_header->framesize);
   ippsCopy_8u((Ipp8u*)pIn->pBuffer,(Ipp8u*)((Ipp8u*)nr_header->tmpBuffer + nr_header->framesize),nr_header->framesize);

   NR_ProcessBuffer(pNRState, (Ipp16s*)nr_header->tmpBuffer, (Ipp16s*)pOut->pBuffer, nr_header->cExpInitFramesLeft);
   pIn->nbytes = nr_header->framesize;
   pOut->nbytes = nr_header->framesize;
   nr_header->cExpInitFramesLeft >>= 1;
   if(pDecision) {
      *pDecision = ACTIVE;
   }
   return USC_NoError;
}
