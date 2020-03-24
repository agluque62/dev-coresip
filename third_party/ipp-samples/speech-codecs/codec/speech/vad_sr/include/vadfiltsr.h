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
//  This header source file contains the coefficients (in Q15 format) of a bandpass filter
//  (70-1000Hz) for 16kHz sampling frequency
//
*/

#ifndef __VADFILT_H__
#define __VADFILT_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
// 4th order IIR Bi-Quad lowpass filter with cutoff frequency 1000Hz
*/
#define NUMBIQUAD  2

Ipp16s pTaps[] = {
    500,  1001,  500,  14,  -22366,   7824,
    500,  1001,  500,  14,  -26406,  12198
};


#ifdef __cplusplus
}
#endif


#endif /* __VADFILT_H__ */

/* EOF */
