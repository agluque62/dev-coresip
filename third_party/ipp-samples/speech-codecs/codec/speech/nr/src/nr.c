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
//      This file contains the functions implementing an Ephraim-Malah noise
//      suppressor.
//
*/

#include "nr.h"
#include <ipps.h>

/*static own functions*/

static void ownNR_InterpolateC_32s_I( const Ipp32s *pSrcOld,
                   Ipp32s *pSrcpDst, Ipp32s len, Ipp32s weightQ31)
{
   Ipp32s i;
   Ipp32s w0=weightQ31,w1=(IPP_MAX_32S-weightQ31)+1;
   volatile Ipp64s sum;

   for (i=0; i<len; i++) {
      sum=(Ipp64s)(pSrcOld[i])*w0+(Ipp64s)(pSrcpDst[i])*w1+(1<<30);
      pSrcpDst[i] = (Ipp32s) (sum>>31);
   }

   return;
}

static void ownNR_InterpolateC_32s( const Ipp32s *pSrcOld,
                   const Ipp32s *pSrcNew, Ipp32s *pDst, Ipp32s len,
                   Ipp32s weightQ31)
{
   Ipp32s i;
   Ipp32s w0=weightQ31,w1=(IPP_MAX_32S-weightQ31)+1;
   volatile Ipp64s sum;

   for (i=0; i<len; i++) {
      sum=(Ipp64s)(pSrcNew[i])*w0+(Ipp64s)(pSrcOld[i])*w1+(1<<30);
      pDst[i] = (Ipp32s) (sum>>31);
   }

   return;
}

static void ownCrossCorr_32s_Sfs(const Ipp32s* pSrc1, const Ipp32s* pSrc2, int srcLen,
                                                   Ipp32s* pDst, int dstLen, int scaleFactor)
{
   Ipp64s sumFIR;
   int i,j;

   if(scaleFactor < 0) return;
   for (i = 0; i < dstLen; i++) {
      sumFIR = 0;

      for (j = 0; j < srcLen; j++) {
         sumFIR += ((Ipp64s)pSrc1[j] * pSrc2[i+j]);
      }

      sumFIR = sumFIR >> scaleFactor;

      if (sumFIR > IPP_MAX_32S) {
         pDst[i] = IPP_MAX_32S;
      } else if (sumFIR < IPP_MIN_32S) {
         pDst[i] = IPP_MAX_32S;
      } else {
         pDst[i] = (Ipp32s)sumFIR;
      }
   }
   return;
}

#define FFT_ORDER        7          /* real and complex FFT order */

Ipp32s sumScale_32s(Ipp32s* pVector, uint cBins, Ipp32s* pScaleFactor);
Ipp32s speechPresenceQ31_32s(Ipp32s* pCleanSpeechPSD, Ipp32s* pNoisePSD, uint cBins);
void   applyMNS(Ipp32s* pEMNRCoefsQ31, Ipp32s* pWienerCoefsQ31, uint cBins, NRstate* pNRState);

/********************************************************************************
// Name:             NR_ProcessBuffer
// Description:      Processes a block of noisy speech to reduce the noise and
//                   produces one half-block of noise-suppressed speech
//                   corresponding to the first half of the input buffer.
// Input Arguments:
// Input/Output Arguments:
//   pNRState       - pointer to NRstate structure
//   pInputBuffer   - pointer to input buffer containing noisy speech samples
//   useAvgNoisePSD - boolean indicating to use averaging instead of full noise
//                    PSD estimation
// Output Arguments:
//   pOutputBuffer  - pointer to output buffer containing noise-suppressed
//                    speech samples
// Returns:  Integer indicating operation status
// Notes:   Only 128-point FFT is included at this time.  Therefore, only 128
//          sample windows are supported.  It is straightforward to extend the
//          example to other window and FFT sizes.
********************************************************************************/
void NR_ProcessBuffer(
  NRstate* pNRState,
  Ipp16s*  pInputBuffer,
  Ipp16s*  pOutputBuf,
  uint     useAvgNoisePSD)
{
  LOCAL_ALIGN_ARRAY(32,Ipp32sc,pNoisySpeechDFT,(MAX_BLOCK_SAMPLES/2)+1,pNRState);
  LOCAL_ALIGN_ARRAY(32,Ipp32s,pNoisySpeechMagSqrDFT,(MAX_BLOCK_SAMPLES/2)+1,pNRState);
  LOCAL_ALIGN_ARRAY(32,Ipp32s,pCleanSpeechPSD,MAX_BLOCK_SAMPLES,pNRState);
  LOCAL_ALIGN_ARRAY(32,Ipp32s,pFilterCoefsQ31,MAX_BLOCK_SAMPLES,pNRState);
  LOCAL_ALIGN_ARRAY(32,Ipp32s,pWienerCoefsQ31,MAX_BLOCK_SAMPLES,pNRState);
  uint    cBins;

  cBins = pNRState->cFrameSamps / 2 + 1;

  /*
  // **********************************************
  // Windowed DFT
  // **********************************************
  */
  ippsZero_32sc(pNoisySpeechDFT,(MAX_BLOCK_SAMPLES/2)+1);
  ippsZero_32s(pNoisySpeechMagSqrDFT,(MAX_BLOCK_SAMPLES/2)+1);
  ippsZero_32s(pCleanSpeechPSD,MAX_BLOCK_SAMPLES);
  ippsZero_32s(pFilterCoefsQ31,MAX_BLOCK_SAMPLES);
  ippsZero_32s(pWienerCoefsQ31,MAX_BLOCK_SAMPLES);
  {
    LOCAL_ALIGN_ARRAY(32,Ipp16s,pWindowedSamples,MAX_BLOCK_SAMPLES,pNRState);

    ippsZero_16s(pWindowedSamples,MAX_BLOCK_SAMPLES);

    /* Apply square root of window function */
    ippsMul_16s_Sfs(pNRState->pWindowQ15, pInputBuffer, pWindowedSamples, pNRState->cFrameSamps, Q15_SF);

    /* Apply discrete Fourier transform (DFT) */
    ippsFFTFwd_RToCCS_16s32s_Sfs(pWindowedSamples, (Ipp32s*)pNoisySpeechDFT, pNRState->pFFTR, 0, pNRState->pBuffer);
    LOCAL_ALIGN_ARRAY_FREE(32,Ipp16s,pWindowedSamples,MAX_BLOCK_SAMPLES,pNRState);
  }

  /*
  // *********************************************************
  // Update noisy speech PSD
  // *********************************************************
  */
  /* Compute magnitude squared of noisy signal DFT bins */
  ippsMagSquared_32sc32s_Sfs(pNoisySpeechDFT, pNoisySpeechMagSqrDFT, cBins, MAG_SQR_FREQ_SF);

  /* Smooth (in time) to estimate the noisy speech power spectral density (PSD) */
  ownNR_InterpolateC_32s_I(pNoisySpeechMagSqrDFT, pNRState->pNoisySpeechPSD, cBins, pNRState->betaQ31);

  /* Enforce lower bound */
  ippsThreshold_LT_32s_I(pNRState->pNoisySpeechPSD, cBins, MIN_NOISY_PSD);

  /*
  // ***********************************************************
  // Update noise PSD estimate
  // ***********************************************************
  */
  if (useAvgNoisePSD)
  {
    /* Assuming the input contains only noise, averaging is applied. */
    ownNR_InterpolateC_32s_I(pNoisySpeechMagSqrDFT, pNRState->pNoisePSD, cBins, NOISE_AVG_WGT_Q31);
  }
  else
  {
    /* Otherwise a more sophisticated approach is used. */
    ippsUpdateNoisePSDMCRA_32s_I(pNoisySpeechMagSqrDFT, pNRState->pMCRAState, pNRState->pNoisePSD);
  }

  /*
  // *********************************************************
  // Update clean speech PSD
  // *********************************************************
  */
  {
    LOCAL_ALIGN_ARRAY(32,Ipp32s,pCleanSpectSub,MAX_BLOCK_SAMPLES,pNRState);
    LOCAL_ALIGN_ARRAY(32,Ipp32s,pMagSquaredOutput,MAX_BLOCK_SAMPLES,pNRState);

    ippsZero_32s(pCleanSpectSub,MAX_BLOCK_SAMPLES);
    ippsZero_32s(pMagSquaredOutput,MAX_BLOCK_SAMPLES);
    /*
    // Spectral subtraction of noise PSD from noisy speech PSD
    // The following line should be used with Intel(R) IPP 2.0 and higher.
    // ippsSub_32s(pNRState->pNoisySpeechPSD, pNRState->pNoisePSD, pCleanSpectSub, cBins);
    // Unfortunately, that was not available at the time of this release.  Therefore,
    // the following code was used instead.
    */
    ippsSub_32s_Sfs(pNRState->pNoisePSD, pNRState->pNoisySpeechPSD, pCleanSpectSub, cBins, 0);

    /* Enforce lower bound */
    ippsThreshold_LT_32s_I(pCleanSpectSub, cBins, 0);

    /* Calculate magnitude squared of previous filter output */
    ippsMagSquared_32sc32s_Sfs(pNRState->pFilterOutput, pMagSquaredOutput, cBins, MAG_SQR_FREQ_SF);

    /* Smooth (in time) to estimate the clean speech power spectral density (PSD) */
    ownNR_InterpolateC_32s(pCleanSpectSub, pMagSquaredOutput, pCleanSpeechPSD, cBins, pNRState->alphaQ31);

    LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pMagSquaredOutput,MAX_BLOCK_SAMPLES,pNRState);
    LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pCleanSpectSub,MAX_BLOCK_SAMPLES,pNRState);
  }

  {
    LOCAL_ALIGN_ARRAY(32,Ipp32s,pPriorSNRQ15,MAX_BLOCK_SAMPLES,pNRState);
    LOCAL_ALIGN_ARRAY(32,Ipp32s,pPostSNRQ15,MAX_BLOCK_SAMPLES,pNRState);
    Ipp32s lowBinIdx;

    ippsZero_32s(pPriorSNRQ15,MAX_BLOCK_SAMPLES);
    ippsZero_32s(pPostSNRQ15,MAX_BLOCK_SAMPLES);
    /*
    // ************************************************************************
    // Update a priori and a posteriori SNR
    // ************************************************************************
    */
    lowBinIdx = pNRState->nLowBinIdx;

    ippsDiv_32s_Sfs(pNRState->pNoisePSD+lowBinIdx, pNRState->pNoisySpeechPSD+lowBinIdx, pPostSNRQ15+lowBinIdx,
      cBins-lowBinIdx, -Q15_SF);

    ippsDiv_32s_Sfs(pNRState->pNoisePSD+lowBinIdx, pCleanSpeechPSD+lowBinIdx, pPriorSNRQ15+lowBinIdx, cBins-lowBinIdx, -Q15_SF);

    /*
    // **************************************************************
    // Update Wiener filter weights
    // **************************************************************
    */
    ippsFilterUpdateWiener_32s(pPriorSNRQ15+lowBinIdx, pWienerCoefsQ31+lowBinIdx, cBins-lowBinIdx);

    /* Enforce lower bound */
    ippsThreshold_LT_32s_I(pWienerCoefsQ31+lowBinIdx, cBins-lowBinIdx, MIN_WIENER_GAIN);

    /*
    // *********************************************************************
    // Update Ephraim-Malah filter weights
    // *********************************************************************
    */
    ippsFilterUpdateEMNS_32s(pWienerCoefsQ31+lowBinIdx, pPostSNRQ15+lowBinIdx, pFilterCoefsQ31+lowBinIdx,
      cBins-lowBinIdx);

    LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pPostSNRQ15,MAX_BLOCK_SAMPLES,pNRState);
    LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pPriorSNRQ15,MAX_BLOCK_SAMPLES,pNRState);
  }

  /*
  // ***************************************************************
  // Update PSD adaptation rates
  // ***************************************************************
  */
  {
    Ipp32s alphaRangeQ31;
    Ipp32s betaRangeQ31;
    Ipp32s product;
    Ipp32s sum;

    /* Calculate noisy speech PSD adaptive step size */
    alphaRangeQ31 = pNRState->alphaMaxQ31 - pNRState->alphaMinQ31;
    ADD_SAT_32S(IPP_MAX_32S, -pNRState->speechPresenceQ31, sum);
    MULT_SCALE_SATQ31_32S(sum, alphaRangeQ31, product);
    ADD_SAT_32S(pNRState->alphaMinQ31, product, pNRState->alphaQ31);

    /* Calculate clean speech PSD adaptive step size */
    betaRangeQ31 = pNRState->betaMaxQ31 - pNRState->betaMinQ31;
    MULT_SCALE_SATQ31_32S(pNRState->speechPresenceQ31, betaRangeQ31, product);
    ADD_SAT_32S(pNRState->betaMinQ31, product, pNRState->betaQ31);
  }

  /*
  // *************************************************************
  // Apply heuristics to filter
  // *************************************************************
  */
  /* Impose high pass filter response by setting the first few filter coefficients to zero */
  ippsZero_16s((Ipp16s*)pFilterCoefsQ31, pNRState->nLowBinIdx * sizeof(Ipp32s) / sizeof(Ipp16s));

  applyMNS(pFilterCoefsQ31+pNRState->nLowBinIdx, pWienerCoefsQ31+pNRState->nLowBinIdx,
    cBins-pNRState->nLowBinIdx, pNRState);

  /*
  // *************************************************************
  // Calculate filter output
  // *************************************************************
  */
  /* Apply output filter coefficient */
  ippsMul_32s32sc_Sfs(pFilterCoefsQ31, pNoisySpeechDFT, pNRState->pFilterOutput, cBins, Q31_SF);

  /*
  // *******************************************************************
  // Update speech presence likelihood
  // *******************************************************************
  */
  pNRState->speechPresenceQ31 = speechPresenceQ31_32s(pCleanSpeechPSD, pNRState->pNoisePSD, cBins);

  /*
  // **************************************************************
  // Inverse DFT and overlap-add
  // **************************************************************
  */
  {
    LOCAL_ALIGN_ARRAY(32,Ipp16s,pNewTimeFilterOutput16s,MAX_BLOCK_SAMPLES,pNRState);
    uint   cHalfFrameSamps;

    ippsZero_16s(pNewTimeFilterOutput16s,MAX_BLOCK_SAMPLES);
    ippsFFTInv_CCSToR_32s16s_Sfs((Ipp32s*)pNRState->pFilterOutput, pNewTimeFilterOutput16s,
       pNRState->pFFTR, 0, pNRState->pBuffer);

    /* Apply square root of window function */
    ippsMul_16s_ISfs(pNRState->pWindowQ15, pNewTimeFilterOutput16s, pNRState->cFrameSamps, Q15_SF);

    cHalfFrameSamps = pNRState->cFrameSamps / 2;

    /* Overlap and add */
    ippsAdd_16s_I(pNRState->pOldTimeFilterOutput16, pNewTimeFilterOutput16s, cHalfFrameSamps);

    /* Save second half of output block for next iteration */
    ippsCopy_16s(pNewTimeFilterOutput16s+cHalfFrameSamps, pNRState->pOldTimeFilterOutput16, cHalfFrameSamps);

    /* Copy first half of output block to output buffer */
    ippsCopy_16s(pNewTimeFilterOutput16s, pOutputBuf, cHalfFrameSamps);

    LOCAL_ALIGN_ARRAY_FREE(32,Ipp16s,pNewTimeFilterOutput16s,MAX_BLOCK_SAMPLES,pNRState);
  }
  LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pWienerCoefsQ31,MAX_BLOCK_SAMPLES,pNRState);
  LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pFilterCoefsQ31,MAX_BLOCK_SAMPLES,pNRState);
  LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pCleanSpeechPSD,MAX_BLOCK_SAMPLES,pNRState);
  LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pNoisySpeechMagSqrDFT,(MAX_BLOCK_SAMPLES/2)+1,pNRState);
  LOCAL_ALIGN_ARRAY_FREE(32,Ipp32sc,pNoisySpeechDFT,(MAX_BLOCK_SAMPLES/2)+1,pNRState);
}


/********************************************************************************
// Name:             NR_Init
// Description:      Initializes state structure
// Input Arguments:
//           pMemory        - pointer to memory buffer
// Input/Output Arguments:
//           ppNRState      - pointer to pointer to NRstate structure
// Output Arguments:
//           cSamples       - count of samples within a single frame
//           cSamplesPerSec - input sample rate
// Returns:  Integer indicating operation status
// Notes:    Although this function accepts an arbitrary value of cSamps,
//           only cSamps==128 is currently supported in NR_ProcessBuffer.
********************************************************************************/
void NR_Init(
  NRstate** ppNRState,
  void*     pMemory,
  uint      cSamples,
  uint      cSamplesPerSec)
{
  NRstate* pState;
  uint     cBins;
  uint     i;
#ifdef __OLD_SAMPLE
  Ipp32s   rSize;
#endif

  /* Initialize state pointer */
  *ppNRState = (NRstate *) pMemory;
  pState = *ppNRState;

  /* Initialize array pointers */
  cBins = cSamples / 2 + 1;
  pState->pNoisySpeechPSD = (Ipp32s*)((Ipp8s*)pMemory + sizeof(NRstate));
  pState->pNoisePSD = (Ipp32s*)(pState->pNoisySpeechPSD + cBins);
  pState->pFilterOutput = (Ipp32sc*)(pState->pNoisePSD + cBins);
  pState->pOldTimeFilterOutput16 = (Ipp16s*)(pState->pFilterOutput + cBins);
  pState->pWindowQ15 = (Ipp16s*)(pState->pOldTimeFilterOutput16 + cSamples);
  pState->pMCRAState = (IppMCRAState*)(pState->pWindowQ15 + cSamples);

  /* Initialize state values */
  pState->cFrameSamps = cSamples;
  pState->speechPresenceQ31 = INITIAL_SPEECH_PRESENCE;
  pState->alphaMaxQ31 = ALPHA_MAX_Q31;
  pState->alphaMinQ31 = ALPHA_MIN_Q31;
  pState->alphaQ31    = ALPHA_MIN_Q31;
  pState->betaMaxQ31  = BETA_MAX_Q31;
  pState->betaMinQ31  = BETA_MIN_Q31;
  pState->betaQ31     = BETA_MIN_Q31;
  pState->nLowBinIdx  = 2 * LOW_CUTOFF_HZ * cBins / cSamplesPerSec;
  pState->smoothSpeechPresenceQ31 = 0;

  /* Intialize MCRA state structure */
  ippsInitMCRA_32s(cSamplesPerSec, cSamples, pState->pMCRAState);

  /* Calculate root triangular window */
  for(i = 0; i < cSamples / 2; i++)
  {
    pState->pWindowQ15[i] = (Ipp16s)((Ipp32s)(2*i+1) * (1<<Q15_SF) / cSamples) / 2;
    pState->pWindowQ15[cSamples-i-1] = pState->pWindowQ15[i];
  }
  ippsSqrt_16s_ISfs(pState->pWindowQ15, cSamples, NUM_SHIFT_Q15_SQRT);

  /* Initial noise estimate and PSD vectors */
  ippsSet_32s(INITIAL_NOISE_ESTIMATE, pState->pNoisePSD, cBins);
  ippsZero_32s(pState->pNoisySpeechPSD, cBins);

  /* Initialize filter output */
  ippsZero_16s((Ipp16s*)pState->pFilterOutput, cBins * sizeof(Ipp32sc) / sizeof(Ipp16s));
  ippsZero_16s((Ipp16s*)pState->pOldTimeFilterOutput16, cSamples);

  /* Determine the number of right shifts needed to prevent overflow when summing cBins values. */
  pState->sumScaleFactor = 0;
  {
    Ipp32s remainder;

    remainder = cBins;

    while(remainder > 0)
    {
      remainder >>= 1;
      pState->sumScaleFactor++;
    }
  }
#ifdef __OLD_SAMPLE
  ippsFFTInitAlloc_R_16s32s(&(pState->pFFTR),FFT_ORDER,IPP_FFT_DIV_INV_BY_N,ippAlgHintAccurate);
  ippsFFTGetBufSize_R_16s32s(pState->pFFTR,&rSize);
  pState->pBuffer=ippsMalloc_8u(rSize);
#else
  {
     Ipp32s stateSize=0;
     Ipp32s SizeSpec, SizeInit, SizeBuf;

     ippsFFTGetSize_R_16s32s(FFT_ORDER,IPP_FFT_DIV_INV_BY_N,ippAlgHintAccurate, &SizeSpec, &SizeInit, &SizeBuf);

     ippsGetSizeMCRA_32s(cSamples, &stateSize);
     pState->pMemSpec = (Ipp8u*)((Ipp8u*)pState->pMCRAState + stateSize);
     pState->pFFTInitBuff = (Ipp8u*)((Ipp8u*)pState->pMemSpec + SizeSpec);
     pState->pBuffer = (Ipp8u*)((Ipp8u*)pState->pFFTInitBuff + SizeInit);
     ippsFFTInit_R_16s32s( &(pState->pFFTR),
                     FFT_ORDER,IPP_FFT_DIV_INV_BY_N,ippAlgHintAccurate,
                     pState->pMemSpec, pState->pFFTInitBuff);
  }
#endif
}


/********************************************************************************
// Name:             NR_FreeState
// Description:      Freen externally allocated memory
// Arguments:
//                  NRstate - pointer to an AECState structure
// Returns:         None
********************************************************************************/
void NR_FreeState(NRstate *pNRstate)
{
#ifdef __OLD_SAMPLE
  if (pNRstate->pFFTR) ippsFFTFree_R_16s32s(pNRstate->pFFTR);
  if (pNRstate->pBuffer) ippsFree(pNRstate->pBuffer);
  free(pNRstate);
#else
  pNRstate = pNRstate;
#endif
}



/********************************************************************************
// Name:             NR_GetMemoryReq
// Description:      Calculate and return the bytes of memory required for NR
//                   based on parameters
// Input Arguments:
//   cSamples          - number of samples in a block
// Output Arguments:
//   pNumBytesRequired - pointer to number of bytes needed by NR
// Returns:  Integer indicating operation status
// Notes:
********************************************************************************/
void NR_GetMemoryReq(
  uint cSamples,
  Ipp32s* pNumBytesRequired)
{
  uint cBins;
  Ipp32s  stateSize;

  cBins = cSamples/2 + 1;
  *pNumBytesRequired = 0;

  /* Add space for state structure */
  *pNumBytesRequired += sizeof(NRstate);

  /* Add space for pNoisySpeechPSD */
  *pNumBytesRequired += sizeof(Ipp32s) * cBins;

  /* Add space for pNoisePSD */
  *pNumBytesRequired += sizeof(Ipp32s) * cBins;

  /* Add space for pFilterOutput */
  *pNumBytesRequired += sizeof(Ipp32sc) * cBins;

  /* Add space for pOldTimeFilterOutput16 */
  *pNumBytesRequired += sizeof(Ipp16s) * cSamples;

  /* Add space for pWindowQ15 */
  *pNumBytesRequired += sizeof(Ipp16s) * cSamples;

  ippsGetSizeMCRA_32s(cSamples, &stateSize);
  *pNumBytesRequired += stateSize;
#ifndef __OLD_SAMPLE
  {
     Ipp32s SizeSpec, SizeInit, SizeBuf;
     ippsFFTGetSize_R_16s32s(FFT_ORDER,IPP_FFT_DIV_INV_BY_N,ippAlgHintAccurate, &SizeSpec, &SizeInit, &SizeBuf);
     *pNumBytesRequired += SizeSpec;
     *pNumBytesRequired += SizeInit;
     *pNumBytesRequired += SizeBuf;
  }
#endif
}

int NR_GetLocalMemoryReq(Ipp32s *scratchSize)
{
   int size = 0;
   if(NULL==scratchSize) return -1;
   size = sizeof(Ipp32sc) * ((MAX_BLOCK_SAMPLES/2)+1) +
          sizeof(Ipp32s) * ((MAX_BLOCK_SAMPLES/2)+1) +
          5 * sizeof(Ipp32s) * (MAX_BLOCK_SAMPLES) +
          sizeof(Ipp32s) * (MAX_BLOCK_SAMPLES+SMOOTH_WIN_WIDTH);

   *scratchSize = size +
                  8 * 32 + /*Number of buffers * max align bytes*/
                  40 * sizeof(Ipp8s*);/*Max scratch blocks*/
   return 0;
}

int NR_InitBuff(NRstate* pNRState, Ipp8s* pMemory)
{
   int size = 0;
   if(NULL == pNRState) return -1;
   if(NULL == pMemory) return -1;

   NR_GetLocalMemoryReq(&size);

   pNRState->Mem.base = pMemory;
   pNRState->Mem.CurPtr = pNRState->Mem.base;
   pNRState->Mem.VecPtr = (Ipp32s *)(pNRState->Mem.base+(size - 40 * sizeof(Ipp8s*)));
   pNRState->Mem.offset = 0;

   return 0;
}

int NR_ReInitBuff(NRstate* pNRState)
{
   pNRState->Mem.CurPtr = pNRState->Mem.base;
   pNRState->Mem.offset = 0;
   return 0;
}
/********************************************************************************
// Name:             SumScale_32s
// Description:      Helper function to sum the elements of a vector and return
//                   the scaled result along with the number of right shifts
//                   needed to scale the sum.
// Input Arguments:
//   pVector           - pointer to input vector
//   len               - length of input vector
// Output Arguments:
//   pScaleFactor      - pointer to number of right shifts used to scale result
// Returns:  scaled sum
// Notes:
********************************************************************************/
Ipp32s sumScale_32s(
  Ipp32s* pVector,
  uint     cBins,
  Ipp32s*    pScaleFactor)
{
   Ipp32s quickSum;

   *pScaleFactor = 0;

   ippsSum_32s_Sfs(pVector, cBins, &quickSum, 0);

   if((quickSum == IPP_MAX_32S) || (quickSum == IPP_MIN_32S)) {
      /*Reculc sum to get scalefactor*/
      uint    i;
      Ipp64s sum64s = 0;

      for(i = 0; i < cBins; i++) {
         sum64s += pVector[i];
      }

      while((sum64s > IPP_MAX_32S) || (sum64s < IPP_MIN_32S)) {
         sum64s >>= 1;
         *pScaleFactor += 1;
      }
      quickSum = (Ipp32s)sum64s;
   }

   return quickSum;
}

/********************************************************************************
// Name:             speechPresenceQ31_32s
// Description:      Helper function to compute speech presence likelihood
//                   from noise and speech PSDs.
// Input Arguments:
//   pCleanSpeechPSD - pointer to clean speech PSD vector
//   pNoisePSD       - pointer to noise PSD vector
//   len             - number of elements in PSD vectors
// Output Arguments:
// Returns:  speech presence likelihood as Q31 value
// Notes:
********************************************************************************/
Ipp32s speechPresenceQ31_32s(
  Ipp32s* pCleanSpeechPSD,
  Ipp32s* pNoisePSD,
  uint     cBins)
{
  Ipp32s noiseEnergy;
  Ipp32s speechEnergy;
  Ipp32s sumEnergy;
  Ipp32s speechPresenceQ31;
  Ipp32s speechScaleFactor;
  Ipp32s noiseScaleFactor;

  /* estimate total speech and noise energy */
  speechEnergy = sumScale_32s(pCleanSpeechPSD, cBins, &speechScaleFactor);

  noiseEnergy = sumScale_32s(pNoisePSD, cBins, &noiseScaleFactor);

  /* adjust values so that scale factors match */
  if (speechScaleFactor > noiseScaleFactor)
  {
    noiseEnergy >>= (speechScaleFactor - noiseScaleFactor);
  }
  else if (noiseScaleFactor > speechScaleFactor)
  {
    speechEnergy >>= (noiseScaleFactor - speechScaleFactor);
  }

  /* compute sum while scaling to avoid overflow */
  sumEnergy = speechEnergy/2 + noiseEnergy/2;
  speechEnergy /= 2;

  /* avoid divide by zero */
  if (sumEnergy > 0)
  {
    ippsDiv_32s_Sfs(&sumEnergy, &speechEnergy, &speechPresenceQ31, 1, -Q31_SF);
  }
  else
  {
    speechPresenceQ31 = 0;
  }

  return(speechPresenceQ31);
}

/********************************************************************************
// Name:             applyMNS
// Description:      Apply heuristic transformation to filter coefficients
//                   to attempt musical noise suppression (MNS).
// Input Arguments:
//   pEMNRCoefsQ31   - pointer to Ephraim-Malah filter coefficients
//   pWienerCoefsQ31 - pointer to Wiener filter coefficients
//   cBins           - number of filter coefficients
// Output Arguments:
// Returns:  none
// Notes:
********************************************************************************/
void applyMNS(
  Ipp32s* pEMNRCoefsQ31,
  Ipp32s* pWienerCoefsQ31,
  uint     cBins,
  NRstate* pNRState)
{
  LOCAL_ALIGN_ARRAY(32,Ipp32s,pFilterCoefsPauseQ31,MAX_BLOCK_SAMPLES,pNRState);
  LOCAL_ALIGN_ARRAY(32,Ipp32s,pFilterCoefsSpeechQ31,MAX_BLOCK_SAMPLES,pNRState);
  LOCAL_ALIGN_ARRAY(32,Ipp32s,pFilterCoefsCopyQ31,MAX_BLOCK_SAMPLES+SMOOTH_WIN_WIDTH,pNRState);
  Ipp32s halfWinWidth;

  ippsZero_32s(pFilterCoefsPauseQ31,MAX_BLOCK_SAMPLES);
  ippsZero_32s(pFilterCoefsSpeechQ31,MAX_BLOCK_SAMPLES);
  ippsZero_32s(pFilterCoefsCopyQ31,MAX_BLOCK_SAMPLES+SMOOTH_WIN_WIDTH);

  /* update smoothed speech presence likelihood */
  pNRState->smoothSpeechPresenceQ31 = (pNRState->speechPresenceQ31>>2)
    + (Ipp32s)(( ((pNRState->smoothSpeechPresenceQ31)>>2) * 3));

  /* average Wiener and E-M gains to reduce flutter during speech */
  ownNR_InterpolateC_32s(pWienerCoefsQ31, pEMNRCoefsQ31, pFilterCoefsSpeechQ31,
    cBins, MNS_WGT_SPEECH_Q31);

  /* apply constant attenuation during non-speech regions */
  ippsRShiftC_32s(pEMNRCoefsQ31, MNS_SHIFTS_PAUSE, pFilterCoefsPauseQ31, cBins);

  /* apply soft speech/pause decision */
  ownNR_InterpolateC_32s(pFilterCoefsPauseQ31, pFilterCoefsSpeechQ31,
    pEMNRCoefsQ31, cBins, pNRState->speechPresenceQ31);

  /* apply filter coefficient smoothing when speech presence is unclear */
  if ((pNRState->smoothSpeechPresenceQ31 > LOW_THRESHOLD_Q31)
    && (pNRState->smoothSpeechPresenceQ31 < HIGH_THRESHOLD_Q31))
  {
      halfWinWidth = SMOOTH_WIN_WIDTH / 2;

      /* clear pFilterCoefsCopyQ31[] to zero */
      ippsZero_16s((Ipp16s*)pFilterCoefsCopyQ31,
      (cBins + SMOOTH_WIN_WIDTH) * sizeof(Ipp32s) / sizeof(Ipp16s));

      /* copy pEMNRCoefsQ31 into pFilterCoefsCopyQ31 */
      ippsCopy_16s((Ipp16s*)pEMNRCoefsQ31, (Ipp16s*)(pFilterCoefsCopyQ31 + halfWinWidth),
      cBins * sizeof(Ipp32s) / sizeof(Ipp16s));

      /* perform FIR smoothing in frequency. Dst is in Q31 format*/
      ownCrossCorr_32s_Sfs(Smooth_FIR_Table, pFilterCoefsCopyQ31, halfWinWidth+halfWinWidth+1,
                                                   pEMNRCoefsQ31, cBins, 31);
  }
  LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pFilterCoefsCopyQ31,MAX_BLOCK_SAMPLES+SMOOTH_WIN_WIDTH,pNRState);
  LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pFilterCoefsSpeechQ31,MAX_BLOCK_SAMPLES,pNRState);
  LOCAL_ALIGN_ARRAY_FREE(32,Ipp32s,pFilterCoefsPauseQ31,MAX_BLOCK_SAMPLES,pNRState);
}

/* EOF */
