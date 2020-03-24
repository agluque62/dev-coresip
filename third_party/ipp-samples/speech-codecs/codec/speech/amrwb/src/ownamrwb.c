/*/////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2005-2007 Intel Corporation. All Rights Reserved.
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
// Purpose: AMRWB speech codec: Miscellaneous functions.
//
*/

#include "ownamrwb.h"

static __ALIGN32 CONST Ipp16s powTbl[33] =
{
 16384, 16743, 17109, 17484, 17867, 18258, 18658, 19066, 19484, 19911, 20347,
 20792, 21247, 21713, 22188, 22674, 23170, 23678, 24196, 24726, 25268, 25821,
 26386, 26964, 27554, 28158, 28774, 29405, 30048, 30706, 31379, 32066, 32767
};

Ipp32s ownPow2_AMRWB(        /* result             [0,0x7fffffff] */
  Ipp16s valExp,           /* Integer part.      val<=30        */
  Ipp16s valFrac           /* Fractional part.   [0.0,1.0)      */
)
{
  Ipp16s exp, index, a, diff;
  Ipp32s s, s2, s3;

  index = (Ipp16s)(valFrac >> 10);
  a = (Ipp16s)((valFrac << 5) & 0x7fff);

  s = powTbl[index] << 16;
  diff = (Ipp16s)((powTbl[index] - powTbl[index+1]) << 1);
  s -= diff * a;

  exp = (Ipp16s)(30 - valExp);

  if (exp >= 31)
    s2 = (s < 0) ? -1 : 0;
  else
    s2 = s >> exp;

  s3 = 1 << (exp - 1);
  if (s & s3) s2++;

  return(s2);
}

void ownAutoCorr_AMRWB_16s32s(Ipp16s *pSrcSignal, Ipp32s valLPCOrder, Ipp32s *pDst,
                              const Ipp16s* tblWindow, Ipp32s windowLen)
{
    Ipp16s valShift;
    Ipp32s i, valSum, tmp;
    IPP_ALIGNED_ARRAY (16, Ipp16s, y, HR_WINDOW_SIZE);

    ippsMul_NR_16s_Sfs(pSrcSignal, tblWindow, y, windowLen, 15);

    valSum = (Ipp32s)16 << 16;
    for (i = 0; i < windowLen; i++)
        valSum += (y[i] * y[i]) >> 7;

    valShift = (Ipp16s)(4 - (Exp_32s(valSum) >> 1));
    if (valShift < 0) valShift = 0;

    for (i = 0; i < windowLen; i++)
        y[i] = ShiftR_NR_16s(y[i], valShift);

    ippsAutoCorr_NormE_16s32s(y, windowLen, pDst, valLPCOrder+1, &tmp);
}

void ownLagWindow_AMRWB_32s_I(Ipp32s *pSrcDst, Ipp32s len)
{
    Ipp32s i;

    for (i = 1; i <= len; i++)
    {
        pSrcDst[i] = Mul_32s(pSrcDst[i]>>1, LagWindowTbl[i - 1]>>1);
    }
}

void ownScaleSignal_AMRWB_16s_ISfs(Ipp16s *pSrcDstSignal, Ipp32s len, Ipp16s ScaleFactor)
{
   Ipp32s i, tmp;

   if (ScaleFactor > 0) {
      for (i=0; i<len; i++) {
         pSrcDstSignal[i] = ShiftL_16s(pSrcDstSignal[i], (Ipp16u)ScaleFactor);
      }
   } else {
      ScaleFactor = (Ipp16s)(-ScaleFactor);
      for (i=0; i<len; i++) {
         tmp = ((Ipp32s)pSrcDstSignal[i]) << (16-ScaleFactor);
         pSrcDstSignal[i] = Cnvrt_NR_32s16s(tmp);
      }
   }
}

#define FAC4   4
#define FAC5   5
#define INV_FAC5   6554
#define DOWN_FAC  26215
#define UP_FAC    20480

static void ownDownSampling(Ipp16s * pSrcSignal, Ipp16s * pDstSignal, Ipp16s len)
{
   Ipp16s intPart, fracPart, pos;
   Ipp32s j, valSum;

   pos = 0;
   for (j = 0; j < len; j++) {
      intPart = (Ipp16s)(pos >> 2);
      fracPart = (Ipp16s)(pos & 3);
      ippsDotProd_16s32s_Sfs( &pSrcSignal[intPart- NB_COEF_DOWN + 1], &FirDownSamplTbl[(3-fracPart)*30],2*NB_COEF_DOWN, &valSum, -2);
      pDstSignal[j] = Cnvrt_NR_32s16s(valSum);
      pos += FAC5;
   }
}

static void ownUpSampling(Ipp16s *pSrcSignal, Ipp16u *pDstSignal, Ipp16s len)
{
   Ipp16s intPart, pos, fracPart;
   Ipp32s j, valSum;

   pos = 0;

   for (j = 0; j < len; j++) {
      intPart = (Ipp16s)((pos * INV_FAC5) >> 15);
      fracPart = (Ipp16s)(pos -((intPart << 2) + intPart));
      ippsDotProd_16s32s_Sfs( &pSrcSignal[intPart- NB_COEF_UP + 1], &FirUpSamplTbl[(4-fracPart)*24],2*NB_COEF_UP, &valSum, -2);
      pDstSignal[j] = Cnvrt_NR_32s16s(valSum);
      pos += FAC4;
   }
}

void ownDecimation_AMRWB_16s(Ipp16s *pSrcSignal16k, Ipp16s len, Ipp16s *pDstSignal12k8,
                             Ipp16s *pMem)
{
    Ipp16s lenDst;
    IPP_ALIGNED_ARRAY (16, Ipp16s, signal, L_FRAME16k + (2 * NB_COEF_DOWN));

    ippsCopy_16s(pMem, signal, 2 * NB_COEF_DOWN);
    ippsCopy_16s(pSrcSignal16k, signal + (2 * NB_COEF_DOWN), len);
    lenDst = (Ipp16s)((len*DOWN_FAC)>>15);
    ownDownSampling(signal + NB_COEF_DOWN, pDstSignal12k8, lenDst);
    ippsCopy_16s(signal + len, pMem, 2 * NB_COEF_DOWN);
}

void ownOversampling_AMRWB_16s(Ipp16s *pSrcSignal12k8, Ipp16s len, Ipp16u *pDstSignal16k,
                               Ipp16s *pMem)
{
    Ipp16s lenDst;
    IPP_ALIGNED_ARRAY (16, Ipp16s, signal, SUBFRAME_SIZE+(2*NB_COEF_UP));

    ippsCopy_16s(pMem, signal, 2*NB_COEF_UP);
    ippsCopy_16s(pSrcSignal12k8, signal+(2*NB_COEF_UP), len);
    lenDst = (Ipp16s)((len*UP_FAC)>>14);
    ownUpSampling(signal+NB_COEF_UP, pDstSignal16k, lenDst);
    ippsCopy_16s(signal+len, pMem, 2*NB_COEF_UP);
}

Ipp16s ownMedian5(Ipp16s *x)
{
    Ipp16s x1, x2, x3, x4, x5;
    Ipp16s tmp;

    x1 = x[-2];
    x2 = x[-1];
    x3 = x[0];
    x4 = x[1];
    x5 = x[2];

    if (x2 < x1)
    {
        tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    if (x3 < x1)
    {
        tmp = x1;
        x1 = x3;
        x3 = tmp;
    }
    if (x4 < x1)
    {
        tmp = x1;
        x1 = x4;
        x4 = tmp;
    }
    if (x5 < x1)
    {
        x5 = x1;
    }
    if (x3 < x2)
    {
        tmp = x2;
        x2 = x3;
        x3 = tmp;
    }
    if (x4 < x2)
    {
        tmp = x2;
        x2 = x4;
        x4 = tmp;
    }
    if (x5 < x2) x5 = x2;
    if (x4 < x3) x3 = x4;
    if (x5 < x3) x3 = x5;

    return (x3);
}

static __ALIGN32 CONST Ipp32s sqrTbl[49] =
{
// 32767, 31790, 30894, 30070, 29309, 28602, 27945, 27330, 26755, 26214,
// 25705, 25225, 24770, 24339, 23930, 23541, 23170, 22817, 22479, 22155,
// 21845, 21548, 21263, 20988, 20724, 20470, 20225, 19988, 19760, 19539,
// 19326, 19119, 18919, 18725, 18536, 18354, 18176, 18004, 17837, 17674,
// 17515, 17361, 17211, 17064, 16921, 16782, 16646, 16514, 16384
0x7fff0000,0x7c2e0000, 0x78ae0000, 0x75760000, 0x727d0000, 0x6fba0000, 0x6d290000, 0x6ac20000,
0x68830000,0x66660000, 0x64690000, 0x62890000, 0x60c20000, 0x5f130000, 0x5d7a0000, 0x5bf50000,
0x5a820000,0x59210000, 0x57cf0000, 0x568b0000, 0x55550000, 0x542c0000, 0x530f0000, 0x51fc0000,
0x50f40000,0x4ff60000, 0x4f010000, 0x4e140000, 0x4d300000, 0x4c530000, 0x4b7e0000, 0x4aaf0000,
0x49e70000,0x49250000, 0x48680000, 0x47b20000, 0x47000000, 0x46540000, 0x45ad0000, 0x450a0000,
0x446b0000,0x43d10000, 0x433b0000, 0x42a80000, 0x42190000, 0x418e0000, 0x41060000, 0x40820000,
0x40000000
};

static __ALIGN32 CONST Ipp16s sqrDiffTbl[48] =
{ // sqrDiffTbl[i]=sqrTbl[i]-sqrTbl[i+1]
  1954,  1792,  1648,  1522,  1414,  1314,  1230,  1150,  1082,  1018,
   960,   910,   862,   818,   778,   742,   706,   676,   648,   620,
   594,   570,   550,   528,   508,   490,   474,   456,   442,   426,
   414,   400,   388,   378,   364,   356,   344,   334,   326,   318,
   308,   300,   294,   286,   278,   272,   264,   260
};

void ownInvSqrt_AMRWB_32s16s_I(Ipp32s *valFrac, Ipp16s *valExp)
{
    Ipp16s i, a;

    if (*valFrac <= 0)
    {
        *valExp = 0;
        *valFrac = IPP_MAX_32S;
        return;
    }
    if( (*valExp & 1) == 1 )
          *valFrac = *valFrac >> 1;

    *valExp = (Ipp16s)(-((*valExp - 1) >> 1));

    i = (Ipp16s)(*valFrac >> 25);
    a = (Ipp16s)((*valFrac >> 10) & 0x7fff);
    *valFrac = (sqrTbl[i-16] - (sqrDiffTbl[i-16] * a));
}

/* Check stability on pSrc1 : distance between pSrc2 and pSrc1 */
Ipp16s ownChkStab(Ipp16s *pSrc1, Ipp16s *pSrc2, Ipp32s len)
{
    Ipp16s valStabFac, tmp;
    Ipp32s i, valSum;

    valSum = 0;
    for (i = 0; i < len; i++)
    {
        tmp = (Ipp16s)(pSrc1[i] - pSrc2[i]);
        valSum += tmp * tmp;
    }

    tmp = (Ipp16s)(ShiftL_32s(valSum, 9) >> 16);

    tmp = Mul_16s_Sfs(tmp, 26214, 15);
    tmp = (Ipp16s)(20480 - tmp);

    valStabFac = Cnvrt_32s16s(tmp << 1);
    if (valStabFac < 0) valStabFac = 0;

    return valStabFac;
}

void ownagc2(Ipp16s *pSrcPFSignal, Ipp16s *pDstPFSignal, Ipp16s len)
{
    Ipp16s valExp;
    Ipp16s valGainIn, valGainOut, valGain;
    Ipp32s i, s;
    IPP_ALIGNED_ARRAY (16, Ipp16s, temp, WINDOW_SIZE);

    ippsRShiftC_16s(pDstPFSignal, 2, temp, len);
    ippsDotProd_16s32s_Sfs((const Ipp16s*) temp, (const Ipp16s*)temp, len, &s, -1);
    if (s == 0) return;

    valExp = (Ipp16s)(Exp_32s_Pos(s) - 1);
    if(valExp < 0)
       valGainOut = Cnvrt_NR_32s16s(s >> (-valExp));
    else
       valGainOut = Cnvrt_NR_32s16s(s << valExp);

    ippsRShiftC_16s(pSrcPFSignal, 2, temp, len);
    ippsDotProd_16s32s_Sfs((const Ipp16s*) temp, (const Ipp16s*)temp, len, &s, -1);
    if (s == 0)
        valGain = 0;
    else
    {
        i = Norm_32s_I(&s);
        valGainIn = Cnvrt_NR_32s16s(s);
        valExp = (Ipp16s)(valExp - i);

        if(valGainOut==valGainIn)
            s = IPP_MAX_16S;
        else
            s = ((valGainOut << 15) / valGainIn);
        s = ShiftL_32s(s, 7);
        if (valExp < 0)
            s = ShiftL_32s(s, (Ipp16u)(-valExp));
        else
            s >>= valExp;

        valExp = (Ipp16s)(31 - Norm_32s_I(&s));
        ownInvSqrt_AMRWB_32s16s_I(&s, &valExp);

        if(valExp > 0)
          s = ShiftL_32s(s, valExp);
        else
          s = s >> (-valExp);

        valGain = Cnvrt_NR_32s16s(ShiftL_32s(s, 9));
    }
    ownMulC_16s_ISfs(valGain, pDstPFSignal, len, 13);
}

void HighPassFIRGetSize_AMRWB_16s_ISfs(Ipp32s *pDstSize)
{
   *pDstSize = sizeof(HighPassFIRState_AMRWB_16s_ISfs);
   *pDstSize = (*pDstSize+7)&(~7);
   return;
}

void HighPassFIRInit_AMRWB_16s_ISfs(Ipp16s *pSrcFilterCoeff, Ipp32s ScaleFactor,
                                    HighPassFIRState_AMRWB_16s_ISfs *pState)
{
   ippsZero_16s(pState->pFilterMemory, HIGH_PASS_MEM_SIZE - 1);
   ippsCopy_16s(pSrcFilterCoeff, pState->pFilterCoeff, HIGH_PASS_MEM_SIZE);
   pState->ScaleFactor = ScaleFactor;
}

void HighPassFIR_AMRWB_16s_ISfs(Ipp16s *pSrcDstSignal, Ipp32s len,
                                HighPassFIRState_AMRWB_16s_ISfs *pState)
{
    IPP_ALIGNED_ARRAY (16, Ipp16s, x, SUBFRAME_SIZE_16k + (HIGH_PASS_MEM_SIZE - 1));
    ippsCopy_16s(pState->pFilterMemory, x, HIGH_PASS_MEM_SIZE - 1);
    ippsRShiftC_16s(pSrcDstSignal, pState->ScaleFactor, &x[HIGH_PASS_MEM_SIZE - 1], len);
    ippsCrossCorr_NR_16s( pState->pFilterCoeff, x, HIGH_PASS_MEM_SIZE, pSrcDstSignal, len);
    ippsCopy_16s(x + len, pState->pFilterMemory, HIGH_PASS_MEM_SIZE - 1);
}
