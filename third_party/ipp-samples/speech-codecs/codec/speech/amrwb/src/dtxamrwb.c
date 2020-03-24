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
// Purpose: AMRWB speech codec DTX utilities.
//
*/

#include "ownamrwb.h"

static void ownAverageIsfHistory(Ipp16s *pIsfOldvec, Ipp16s *pIndices, Ipp32s *pIsfAvrvec);
static void ownFindFrameIndices(Ipp16s *pIsfOldvec, Ipp16s *pIndices, SDtxEncoderState *st);
static Ipp16s ownDitherCtrl(SDtxEncoderState *st);
static void ownCNDithering(Ipp16s *pIsfvec, Ipp32s *valLogIntEnergy, Ipp16s *pDitherSeed);

Ipp16s ownDTXEncReset(SDtxEncoderState *st, Ipp16s *pIsfInit)
{
    Ipp32s i;

    if (st == (SDtxEncoderState*)NULL) return -1;

    st->siHistPtr = 0;
    st->siLogEnerIndex = 0;

    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        ippsCopy_16s(pIsfInit, &st->asiIsfHistory[i * LP_ORDER], LP_ORDER);
    }
    st->siCngSeed = RND_INIT;

    ippsZero_16s(st->siLogEnerHist, DTX_HIST_SIZE);

    st->siHangoverCount = DTX_HANG_CONST;
    st->siAnaElapsedCount = 32767;

    ippsZero_16s((Ipp16s*)st->aiDistMatrix, 28*2);
    ippsZero_16s((Ipp16s*)st->aiSumDist, (DTX_HIST_SIZE - 1)*2);

    return 1;
}

Ipp16s ownDTXEnc(SDtxEncoderState *st, Ipp16s *pIsfvec, Ipp16s *pExc2vec, Ipp16u *pPrmsvec)
{
    Ipp16s valLogEnergy, valGain, valLevelTmp, valExp, valExpTmp, tmp;
    Ipp16s valLogIntPart, valLogFracPart;
    Ipp32s i, valEner, valLevel;
    Ipp16s pIsfOrdervec[3];
    Ipp16s valCNDith;
    IPP_ALIGNED_ARRAY(16, Ipp32s, pIsf32svec, LP_ORDER);

    /* VOX mode computation of SID parameters */
    ippsZero_16s((Ipp16s*)pIsf32svec, LP_ORDER*2);
    /* average energy and isf */
    /* Division by DTX_HIST_SIZE = 8 has been done in dtx_buffer */
    ippsSum_16s_Sfs(st->siLogEnerHist, DTX_HIST_SIZE, &valLogEnergy, 0);
    ownFindFrameIndices(st->asiIsfHistory, pIsfOrdervec, st);
    ownAverageIsfHistory(st->asiIsfHistory, pIsfOrdervec, pIsf32svec);

    for (i = 0; i < LP_ORDER; i++)
    {
        pIsfvec[i] = (Ipp16s)(pIsf32svec[i] >> 3);
    }

    /* quantize logarithmic energy to 6 bits (-6 : 66 dB) which corresponds to -2:22 in log2(E) */
    valLogEnergy >>= 2;
    valLogEnergy += 512;

    /* Multiply by 2.625 to get full 6 bit range */
    valLogEnergy = Mul_16s_Sfs(valLogEnergy, 21504, 15);

    /* Quantize Energy */
    st->siLogEnerIndex = (Ipp16s)(valLogEnergy >> 6);
    if (st->siLogEnerIndex > 63) st->siLogEnerIndex = 63;
    if (st->siLogEnerIndex < 0) st->siLogEnerIndex = 0;

    /* Quantize ISFs */
    ippsISFQuantDTX_AMRWB_16s(pIsfvec, pIsfvec, (Ipp16s*)pPrmsvec);
    pPrmsvec += 5;

    *(pPrmsvec)++ = st->siLogEnerIndex;

    valCNDith = ownDitherCtrl(st);
    *(pPrmsvec)++ = valCNDith;

    /* level = (Ipp32f)( pow( 2.0f, (Ipp32f)st->siLogEnerIndex / 2.625 - 2.0 ) );    */
    valLogEnergy = (Ipp16s)(st->siLogEnerIndex << 9);

    /* Divide by 2.625; siLogEnergy will be between 0:24  */
    valLogEnergy = Mul_16s_Sfs(valLogEnergy, 12483, 15);

    valLogIntPart = (Ipp16s)(valLogEnergy >> 10);
    valLogFracPart = (Ipp16s) (valLogEnergy & 0x3ff);
    valLogFracPart <<= 5;
    valLogIntPart += 15;

    valLevel = ownPow2_AMRWB(valLogIntPart, valLogFracPart);
    valExpTmp = (Ipp16s)(15 - Norm_32s_I(&valLevel));
    valLevelTmp = (Ipp16s)(valLevel >> 16);

    /* generate white noise vector */
    for (i = 0; i < FRAME_SIZE; i++)
    {
        pExc2vec[i] = (Ipp16s)(Rand_16s(&(st->siCngSeed)) >> 4);
    }

    /* energy of generated excitation */
    ippsDotProd_16s32s_Sfs( pExc2vec, pExc2vec,FRAME_SIZE, &valEner, -1);
    valEner = Add_32s(valEner, 1);

    valExp = (Ipp16s)(30 - Norm_32s_I(&valEner));
    ownInvSqrt_AMRWB_32s16s_I(&valEner, &valExp);

    valGain = (Ipp16s)(valEner >> 16);
    valGain = Mul_16s_Sfs(valLevelTmp, valGain, 15);

    valExp = (Ipp16s)(valExp + valExpTmp);

    /* Multiply by sqrt(FRAME_SIZE)=16, i.e. shift left by 4 */
    valExp += 4;

    for (i = 0; i < FRAME_SIZE; i++)
    {
        tmp = Mul_16s_Sfs(pExc2vec[i], valGain, 15);
        if (valExp > 0)
            pExc2vec[i] = Cnvrt_32s16s(tmp << valExp);
        else
            pExc2vec[i] = (Ipp16s)(tmp >> (-valExp));
    }

    return 0;
}

Ipp16s ownDTXDecReset(SDtxDecoderState * st, Ipp16s *pIsfInit)
{
    Ipp32s i;

    if (st == (SDtxDecoderState*) NULL) return -1;

    st->siSidLast = 0;
    st->siLogEnergy = 3500;
    st->siLogEnergyOld = 3500;
    st->siSidPeriodInv = (1 << 13);

    /* low level noise for better performance in  DTX handover cases */

    st->siCngSeed = RND_INIT;
    st->siHistPtr = 0;

    ippsCopy_16s(pIsfInit, st->asiIsf, LP_ORDER);
    ippsCopy_16s(pIsfInit, st->asiIsfOld, LP_ORDER);
    ippsSet_16s(st->siLogEnergy, st->siLogEnerHist, DTX_HIST_SIZE);

    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        ippsCopy_16s(pIsfInit, &st->asiIsfHistory[i * LP_ORDER], LP_ORDER);
    }

    st->siAnaElapsedCount = 32767;
    st->siHangoverCount = DTX_HANG_CONST;
    st->siValidData = 0;
    st->siSidFrame = 0;
    st->siGlobalState = SPEECH;
    st->siHangoverAdded = 0;
    st->siDitherSeed = RND_INIT;
    st->siDataUpdated = 0;
    st->siComfortNoiseDith = 0;

    return 0;
}

Ipp16s ownDTXDec(SDtxDecoderState *st, Ipp16s *pExc2vec, Ipp16s valDTXState, Ipp16s *pIsfvec,
                 const Ipp16u *pPrmsvec)
{
    Ipp16s valLogEnergyIndex;
    Ipp16s valIntFac;
    Ipp16s valGain;
    Ipp16s valHistPtr;
    Ipp16s valInterFac;
    Ipp16s tmp, valExp, valExpTmp, valLogIntPart, valLogFracPart, valLevelTmp;
    Ipp32s i, j, valLogIntEnergy, valLevel, valEner;
    IPP_ALIGNED_ARRAY(16, Ipp32s, pIsf32svec, LP_ORDER);

    if ((st->siHangoverAdded != 0) && (st->siSidFrame != 0))
    {
        /* sid_first after dtx hangover period or sid_upd after dtxhangover */
        /* consider  twice the last frame */
        valHistPtr = (Ipp16s)(st->siHistPtr + 1);
        if (valHistPtr == DTX_HIST_SIZE) valHistPtr = 0;

        ippsMove_16s(&st->asiIsfHistory[st->siHistPtr * LP_ORDER], &st->asiIsfHistory[valHistPtr * LP_ORDER], LP_ORDER);
        st->siLogEnerHist[valHistPtr] = st->siLogEnerHist[st->siHistPtr];

        ippsZero_16s((Ipp16s*)pIsf32svec, LP_ORDER*2);

        ippsSum_16s_Sfs(st->siLogEnerHist, DTX_HIST_SIZE, &st->siLogEnergy, 0);
        for (i = 0; i < DTX_HIST_SIZE; i++)
        {
            for (j = 0; j < LP_ORDER; j++)
            {
                pIsf32svec[j] += (Ipp32s)(st->asiIsfHistory[i * LP_ORDER + j]);
            }
        }

        st->siLogEnergy >>= 1;
        st->siLogEnergy += 1024;

        if (st->siLogEnergy < 0) st->siLogEnergy = 0;

        for (j = 0; j < LP_ORDER; j++)
        {
            st->asiIsf[j] = (Ipp16s)(pIsf32svec[j] >> 3);
        }
    }

    if (st->siSidFrame != 0)
    {
        ippsCopy_16s(st->asiIsf, st->asiIsfOld, LP_ORDER);
        st->siLogEnergyOld = st->siLogEnergy;

        if (st->siValidData != 0)           /* new data available (no CRC) */
        {
            valInterFac = st->siSidLast;
            if (valInterFac > 32) valInterFac = 32;
            if (valInterFac >= 2)
                st->siSidPeriodInv = (Ipp16s)((1 << 15) / valInterFac);
            else
                st->siSidPeriodInv = (1 << 14);

            ippsISFQuantDecodeDTX_AMRWB_16s((Ipp16s*)pPrmsvec, st->asiIsf);
            pPrmsvec += 5;

            valLogEnergyIndex = *(pPrmsvec)++;

            /* read background noise stationarity information */
            st->siComfortNoiseDith = *(pPrmsvec)++;
            st->siLogEnergy = (Ipp16s)(valLogEnergyIndex << 9);
            st->siLogEnergy = Mul_16s_Sfs(st->siLogEnergy, 12483, 15);

            if ((st->siDataUpdated == 0) || (st->siGlobalState == SPEECH))
            {
                ippsCopy_16s(st->asiIsf, st->asiIsfOld, LP_ORDER);
                st->siLogEnergyOld = st->siLogEnergy;
            }
        }
    }

    if ((st->siSidFrame != 0) && (st->siValidData != 0))
    {
        st->siSidLast = 0;
    }
    /* Interpolate SID info */
    valIntFac = (Ipp16s)(st->siSidLast << 10);
    valIntFac = Mul_16s_Sfs(valIntFac, st->siSidPeriodInv, 15);

    if (valIntFac > 1024) valIntFac = 1024;
    valIntFac <<= 4;

    valLogIntEnergy = 2 * valIntFac * st->siLogEnergy;
    ownMulC_16s_Sfs(st->asiIsf, valIntFac, pIsfvec, LP_ORDER, 15);

    valIntFac = (Ipp16s)(16384 - valIntFac);

    valLogIntEnergy += 2 * valIntFac * st->siLogEnergyOld;

    for (i = 0; i < LP_ORDER; i++)
    {
        pIsfvec[i] = (Ipp16s)(pIsfvec[i] + Mul_16s_Sfs(valIntFac, st->asiIsfOld[i], 15));
        pIsfvec[i] <<= 1;
    }

    /* If background noise is non-stationary, insert comfort noise dithering */
    if (st->siComfortNoiseDith != 0)
    {
        ownCNDithering(pIsfvec, &valLogIntEnergy, &st->siDitherSeed);
    }
    valLogIntEnergy >>= 9;
    valLogIntPart = (Ipp16s)(valLogIntEnergy >> 16);
    valLogFracPart = (Ipp16s)((valLogIntEnergy - (Ipp32s)(valLogIntPart << 16)) >> 1);
    valLogIntPart += (16 - 1);

    valLevel = ownPow2_AMRWB(valLogIntPart, valLogFracPart);
    valExpTmp = (Ipp16s)(15 - Norm_32s_I(&valLevel));
    valLevelTmp = (Ipp16s)(valLevel >> 16);

    /* generate white noise vector */
    for (i = 0; i < FRAME_SIZE; i++)
    {
        pExc2vec[i] = (Ipp16s)(Rand_16s(&(st->siCngSeed)) >> 4);
    }

    /* energy of generated excitation */
    ippsDotProd_16s32s_Sfs( pExc2vec, pExc2vec,FRAME_SIZE, &valEner, -1);
    valEner = Add_32s(valEner, 1);

    valExp = (Ipp16s)(30 - Norm_32s_I(&valEner));
    ownInvSqrt_AMRWB_32s16s_I(&valEner, &valExp);

    valGain = (Ipp16s)(valEner >> 16);
    valGain = Mul_16s_Sfs(valLevelTmp, valGain, 15);

    valExp = (Ipp16s)(valExp + valExpTmp);
    valExp += 4;

    for (i = 0; i < FRAME_SIZE; i++)
    {
        tmp = Mul_16s_Sfs(pExc2vec[i], valGain, 15);
        if (valExp > 0)
            pExc2vec[i] = (Ipp16s)(ShiftL_32s(tmp, valExp));
        else
            pExc2vec[i] = (Ipp16s)(tmp >> (-valExp));
    }

    if (valDTXState == DTX_MUTE)
    {
        /* mute comfort noise as it has been quite a long time since last SID update  was performed  */
        valInterFac = st->siSidLast;
        if (valInterFac > 32) valInterFac = 32;

        st->siSidPeriodInv = (Ipp16s)((1 << 15) / valInterFac);
        st->siSidLast = 0;
        st->siLogEnergyOld = st->siLogEnergy;
        st->siLogEnergy -= 64;
    }
    /* reset interpolation length timer if data has been updated */

    if ((st->siSidFrame != 0) &&
        ((st->siValidData != 0) || ((st->siValidData == 0) && (st->siHangoverAdded) != 0)))
    {
        st->siSidLast = 0;
        st->siDataUpdated = 1;
    }
    return 0;
}

Ipp16s ownRXDTXHandler(SDtxDecoderState *st, Ipp16s frameType)
{
    Ipp16s valDTXState;

    /* DTX if SID frame or previously in DTX{_MUTE} and (NO_RX OR BAD_SPEECH) */

   if ((frameType == RX_SID_FIRST)  ||
       (frameType == RX_SID_UPDATE) ||
       (frameType == RX_SID_BAD)    ||
       (((st->siGlobalState == DTX) ||
       (st->siGlobalState == DTX_MUTE)) &&
       ((frameType == RX_NO_DATA)   ||
       (frameType == RX_SPEECH_BAD) ||
       (frameType == RX_SPEECH_LOST))))
    {
        valDTXState = DTX;

        if ((st->siGlobalState == DTX_MUTE) &&
            ((frameType == RX_SID_BAD)     ||
             (frameType == RX_SID_FIRST)   ||
             (frameType == RX_SPEECH_LOST) ||
             (frameType == RX_NO_DATA)))
        {
            valDTXState = DTX_MUTE;
        }
        st->siSidLast += 1;

        if (st->siSidLast > DTX_MAX_EMPTY_THRESH)
            valDTXState = DTX_MUTE;
    } else
    {
        valDTXState = SPEECH;
        st->siSidLast = 0;
    }

    if ((st->siDataUpdated == 0) && (frameType == RX_SID_UPDATE))
        st->siAnaElapsedCount = 0;
    st->siAnaElapsedCount = Cnvrt_32s16s(st->siAnaElapsedCount+1);
    st->siHangoverAdded = 0;

    if ((frameType == RX_SID_FIRST)  ||
        (frameType == RX_SID_UPDATE) ||
        (frameType == RX_SID_BAD)    ||
        (frameType == RX_NO_DATA))
    {
       if (st->siAnaElapsedCount > DTX_ELAPSED_FRAMES_THRESH)
       {
            st->siHangoverAdded = 1;
            st->siAnaElapsedCount = 0;
            st->siHangoverCount = 0/*1*/;
       } else if (st->siHangoverCount == 0)
       {
            st->siAnaElapsedCount = 0;
       } else
       {
            st->siHangoverCount -= 1;
       }
    } else
    {
       st->siHangoverCount = DTX_HANG_CONST;
    }

    if (valDTXState != SPEECH)
    {
        /* DTX or DTX_MUTE CN data is not in a first SID, first SIDs are marked as SID_BAD but will do
         * backwards analysis if a hangover period has been added according to the state machine above */

        st->siSidFrame = 0/*2*/;
        st->siValidData = 0;

        if (frameType == RX_SID_FIRST)
        {
            st->siSidFrame = 1;
        } else if (frameType == RX_SID_UPDATE)
        {
            st->siSidFrame = 1;
            st->siValidData = 1;
        } else if (frameType == RX_SID_BAD)
        {
            st->siSidFrame = 1;
            st->siHangoverAdded = 0;
        }
    }
    return valDTXState;
}

static void ownAverageIsfHistory(Ipp16s *pIsfOldvec, Ipp16s *pIndices, Ipp32s *pIsfAvrvec)
{
    Ipp32s i, j, k;
    IPP_ALIGNED_ARRAY(16, Ipp16s, pIsfTmpvec, 2 * LP_ORDER);
    Ipp32s s;

    for (k = 0; k < 2; k++)
    {
        if ((pIndices[k] + 1) != 0)
        {
            ippsCopy_16s(&pIsfOldvec[pIndices[k] * LP_ORDER], &pIsfTmpvec[k * LP_ORDER], LP_ORDER);
            ippsMove_16s(&pIsfOldvec[pIndices[2] * LP_ORDER], &pIsfOldvec[pIndices[k] * LP_ORDER], LP_ORDER);
        }
    }

    for (j = 0; j < LP_ORDER; j++)
    {
        s = 0;
        for (i = 0; i < DTX_HIST_SIZE; i++)
            s += (Ipp32s)(pIsfOldvec[i * LP_ORDER + j]);
        pIsfAvrvec[j] = s;
    }

    for (k = 0; k < 2; k++)
    {
        if ((pIndices[k] + 1) != 0)
            ippsCopy_16s(&pIsfTmpvec[k * LP_ORDER], &pIsfOldvec[pIndices[k] * LP_ORDER], LP_ORDER);
    }
    return;
}

static void ownFindFrameIndices(Ipp16s *pIsfOldvec, Ipp16s *pIndices, SDtxEncoderState *st)
{
    Ipp32s s, valSumMin, valSumMax, valSumMax2;
    Ipp16s i, j, tmp;
    Ipp16s valHistPtr;

    tmp = DTX_HIST_SIZE_MIN_ONE;
    j = -1;
    for (i = 0; i < DTX_HIST_SIZE_MIN_ONE; i++)
    {
        j = (Ipp16s)(j + tmp);
        st->aiSumDist[i] -= st->aiDistMatrix[j];
        tmp -= 1;
    }

    ippsMove_32f((Ipp32f*)st->aiSumDist, (Ipp32f*)&st->aiSumDist[1], DTX_HIST_SIZE_MIN_ONE);
    st->aiSumDist[0] = 0;

    tmp = 0;
    for (i = 27; i >= 12; i = (Ipp16s) (i - tmp))
    {
        tmp += 1;
        ippsMove_32f((Ipp32f*)&st->aiDistMatrix[i - tmp - tmp], (Ipp32f*)&st->aiDistMatrix[i - tmp + 1], tmp);
    }

    valHistPtr = st->siHistPtr;
    for (i = 1; i < DTX_HIST_SIZE; i++)
    {
        /* Compute the distance between the latest isf and the other isfs. */
        valHistPtr -= 1;
        if (valHistPtr < 0) valHistPtr = DTX_HIST_SIZE_MIN_ONE;
        s = 0;
        for (j = 0; j < LP_ORDER; j++)
        {
            tmp = (Ipp16s)(pIsfOldvec[st->siHistPtr * LP_ORDER + j] - pIsfOldvec[valHistPtr * LP_ORDER + j]);
            s += tmp * tmp;
        }
        st->aiDistMatrix[i - 1] = s << 1;
        st->aiSumDist[0] += st->aiDistMatrix[i - 1];
        st->aiSumDist[i] += st->aiDistMatrix[i - 1];
    }

    /* Find the minimum and maximum distances */
    valSumMax = st->aiSumDist[0];
    valSumMin = st->aiSumDist[0];
    pIndices[0] = 0;
    pIndices[2] = 0;
    for (i = 1; i < DTX_HIST_SIZE; i++)
    {
        if (st->aiSumDist[i] > valSumMax)
        {
            pIndices[0] = i;
            valSumMax = st->aiSumDist[i];
        }
        if (st->aiSumDist[i] < valSumMin)
        {
            pIndices[2] = i;
            valSumMin = st->aiSumDist[i];
        }
    }

    /* Find the second largest distance */
    valSumMax2 = IPP_MIN_32S + 1;
    pIndices[1] = -1;
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        if ((st->aiSumDist[i] > valSumMax2) && (i != pIndices[0]))
        {
            pIndices[1] = i;
            valSumMax2 = st->aiSumDist[i];
        }
    }

    for (i = 0; i < 3; i++)
    {
        pIndices[i] = (Ipp16s)(st->siHistPtr - pIndices[i]);
        if (pIndices[i] < 0) pIndices[i] += DTX_HIST_SIZE;
    }

    tmp = Norm_32s_I(&valSumMax);
    valSumMin <<= tmp;
    s = 2 * Cnvrt_NR_32s16s(valSumMax) * INV_MED_THRESH;
    if (s <= valSumMin) pIndices[0] = -1;

    valSumMax2 <<= tmp;
    s = 2 * Cnvrt_NR_32s16s(valSumMax2) * INV_MED_THRESH;
    if (s <= valSumMin) pIndices[1] = -1;

    return;
}

static Ipp16s ownDitherCtrl(SDtxEncoderState *st)
{
    Ipp16s tmp, valMean, valCNDith, valGainDiff;
    Ipp32s i, valIsfDiff;

    /* determine how stationary the spectrum of background noise is */
    ippsSum_32s_Sfs(st->aiSumDist, 8, &valIsfDiff, 0);
//    valIsfDiff = 0;
//    for (i = 0; i < 8; i++)
//    {
//        valIsfDiff = Add_32s(valIsfDiff, st->aiSumDist[i]);
//    }
    if ((valIsfDiff >> 26) > 0)
        valCNDith = 1;
    else
        valCNDith = 0;

    /* determine how stationary the energy of background noise is */
    ippsSum_16s_Sfs(st->siLogEnerHist, DTX_HIST_SIZE, &valMean, 3);
    valGainDiff = 0;
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        tmp = Abs_16s((Ipp16s)(st->siLogEnerHist[i] - valMean));
        valGainDiff = (Ipp16s)(valGainDiff + tmp);
    }
    if (valGainDiff > GAIN_THRESH) valCNDith = 1;

    return valCNDith;
}

static void ownCNDithering(Ipp16s *pIsfvec, Ipp32s *valLogIntEnergy, Ipp16s *pDitherSeed)
{
    Ipp16s temp, temp1, valDitherFac, valRandDither, valRandDither2;
    Ipp32s i;

    /* Insert comfort noise dithering for energy parameter */
    valRandDither = (Ipp16s)(Rand_16s(pDitherSeed) >> 1);
    valRandDither2 = (Ipp16s)(Rand_16s(pDitherSeed) >> 1);
    valRandDither = (Ipp16s)(valRandDither + valRandDither2);
    *valLogIntEnergy += 2 * valRandDither * GAIN_FACTOR;
    if (*valLogIntEnergy < 0) *valLogIntEnergy = 0;

    /* Insert comfort noise dithering for spectral parameters (ISF-vector) */
    valDitherFac = ISF_FACTOR_LOW;
    valRandDither = (Ipp16s)(Rand_16s(pDitherSeed) >> 1);
    valRandDither2 = (Ipp16s)(Rand_16s(pDitherSeed) >> 1);
    valRandDither = (Ipp16s)(valRandDither + valRandDither2);
    temp = (Ipp16s)(pIsfvec[0] + ((valRandDither * valDitherFac + 0x00004000) >> 15));

    if (temp < ISF_GAP)
        pIsfvec[0] = ISF_GAP;
    else
        pIsfvec[0] = temp;

    for (i = 1; i < LP_ORDER - 1; i++)
    {
        valDitherFac += ISF_FACTOR_STEP;
        valRandDither = (Ipp16s)(Rand_16s(pDitherSeed) >> 1);
        valRandDither2 = (Ipp16s)(Rand_16s(pDitherSeed) >> 1);
        valRandDither = (Ipp16s)(valRandDither + valRandDither2);
        temp = (Ipp16s)(pIsfvec[i] + ((valRandDither * valDitherFac + 0x00004000) >> 15));
        temp1 = (Ipp16s)(temp - pIsfvec[i - 1]);

        /* Make sure that isf spacing remains at least ISF_DITHER_GAP Hz */
        if (temp1 < ISF_DITHER_GAP)
            pIsfvec[i] = (Ipp16s)(pIsfvec[i - 1] + ISF_DITHER_GAP);
        else
            pIsfvec[i] = temp;
    }

    /* Make sure that pIsfvec[LP_ORDER-2] will not get values above 16384 */
    if (pIsfvec[LP_ORDER - 2] > 16384) pIsfvec[LP_ORDER - 2] = 16384;
}
