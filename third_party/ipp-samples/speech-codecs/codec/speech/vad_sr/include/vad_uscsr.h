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
//  This header source file contains the function prototypes and structures
//  for a Voice Activity Detector API.
//
*/

#ifndef __VAD_H__
#define __VAD_H__


#ifdef __cplusplus
extern "C" {
#endif


/* headers */
#include <ippdefs.h>

/* VAD internal state structure */
typedef struct _VADStateStruct VADStateStruct;

/* VAD decision states */
typedef enum
{
    NODECISION    = -1,
    ACTIVE        = 0,
    INACTIVE      = 1,
    END_OF_STREAM = 2
}
VADDecisionState;

/* Circular buffer for Ipp16s values */
typedef struct _CircBuffer16s
{
    int      headIdx;  /* position of the head */
    int      tailIdx;  /* position of the tail */
    int      len;      /* length of pSamps */
    Ipp16s*  pBuf;   /* pointer to array of samples */
}
CircBufStruct16s;

/* VAD API Prototypes */

void VAD_GetStateSizeBytes(int cWinSamps, int* pNumStateBytes);

void VAD_Init(VADStateStruct* pState, int frameShiftMsec, int cWinSamps, int sampFreqHz);

void VAD_ProcessFrame(
  VADStateStruct*   pState,
  const Ipp16s*     pInFrame,
  int               len,
  VADDecisionState* pDecisionState,
  int*              pDecisionFrameNum);

void VAD_ProcessEndOfInput(
  VADStateStruct*   pState,
  VADDecisionState* pDecisionState,
  int*              pDecisionFrameNum);


#ifdef __cplusplus
}
#endif


#endif /* __VAD_H__ */

/* EOF */

