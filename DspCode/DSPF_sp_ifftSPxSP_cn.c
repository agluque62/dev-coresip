/* ======================================================================= */
/* DSPF_sp_ifftSPxSP_cn.c -- Inverse FFT with Mixed Radix                  */
/*                 Natural C Implementation                                */
/*                                                                         */
/* Rev 0.0.3                                                               */
/*                                                                         */
/* ----------------------------------------------------------------------- */
/*            Copyright (c) 2009 Texas Instruments, Incorporated.          */
/*                           All Rights Reserved.                          */
/* ======================================================================= */
/* #pragma CODE_SECTION(DSPF_sp_ifftSPxSP_cn, ".text:natural"); */

#include "DSPF_sp_ifftSPxSP_cn.h"
#include <stdio.h>

#ifndef _TMS320C6x

static unsigned int _bitr(unsigned int a)
{
    unsigned int i, b=0;

    for(i=0; i<32; i++)
    {
        b |=  (((a >> i) & 0x1) << (31 -i));
    }

    return(b);
}

#endif


void DSPF_sp_ifftSPxSP_cn (int N, float *ptr_x, float *ptr_w, float *ptr_y,
    unsigned char *brev, int n_min, int offset, int n_max)
{

    int i, j, k, l1, l2, h2, predj;
    int tw_offset, stride, fft_jmp;

    float x0, x1, x2, x3, x4, x5, x6, x7;
    float xt0, yt0, xt1, yt1, xt2, yt2, yt3;
    float yt4, yt5, yt6, yt7;
    float si1, si2, si3, co1, co2, co3;
    float xh0, xh1, xh20, xh21, xl0, xl1, xl20, xl21;
    float x_0, x_1, x_l1, x_l1p1, x_h2, x_h2p1, x_l2, x_l2p1;
    float xl0_0, xl1_0, xl0_1, xl1_1;
    float xh0_0, xh1_0, xh0_1, xh1_1;
    float *x, *w;
    int l0, radix;
    float *y0, *ptr_x0, *ptr_x2;
    float scale;

    radix = n_min;

    stride = N;                                                /* n is the number of complex samples */
    tw_offset = 0;
    while (stride > radix)
    {
        j = 0;
        fft_jmp = stride + (stride >> 1);
        h2 = stride >> 1;
        l1 = stride;
        l2 = stride + (stride >> 1);
        x = ptr_x;
        w = ptr_w + tw_offset;

        for (i = 0; i < N; i += 4)
        {
            co1 = w[j];
            si1 = w[j + 1];
            co2 = w[j + 2];
            si2 = w[j + 3];
            co3 = w[j + 4];
            si3 = w[j + 5];

            x_0 = x[0];
            x_1 = x[1];
            x_h2 = x[h2];
            x_h2p1 = x[h2 + 1];
            x_l1 = x[l1];
            x_l1p1 = x[l1 + 1];
            x_l2 = x[l2];
            x_l2p1 = x[l2 + 1];

            xh0 = x_0 + x_l1;
            xh1 = x_1 + x_l1p1;
            xl0 = x_0 - x_l1;
            xl1 = x_1 - x_l1p1;

            xh20 = x_h2 + x_l2;
            xh21 = x_h2p1 + x_l2p1;
            xl20 = x_h2 - x_l2;
            xl21 = x_h2p1 - x_l2p1;

            ptr_x0 = x;
            ptr_x0[0] = xh0 + xh20;
            ptr_x0[1] = xh1 + xh21;

            ptr_x2 = ptr_x0;
            x += 2;
            j += 6;
            predj = (j - fft_jmp);
            if (!predj)
                x += fft_jmp;
            if (!predj)
                j = 0;

            xt0 = xh0 - xh20;                                
            yt0 = xh1 - xh21;                                
            xt1 = xl0 - xl21;
            yt2 = xl1 - xl20;
            xt2 = xl0 + xl21;
            yt1 = xl1 + xl20;

            ptr_x2[l1] = xt1 * co1 - yt1 * si1;
            ptr_x2[l1 + 1] = yt1 * co1 + xt1 * si1;
            ptr_x2[h2] = xt0 * co2 - yt0 * si2;
            ptr_x2[h2 + 1] = yt0 * co2 + xt0 * si2;
            ptr_x2[l2] = xt2 * co3 - yt2 * si3;
            ptr_x2[l2 + 1] = yt2 * co3 + xt2 * si3;
        }
        tw_offset += fft_jmp;
        stride = stride >> 2;
    }                                                        /* end while */

    j = offset >> 2;

    ptr_x0 = ptr_x;
    y0 = ptr_y;
    /* l0 = _norm(n_max) + 3; get size of fft */
    l0 = 0;
    for (k = 30; k >= 0; k--)
        if ((n_max & (1 << k)) == 0)
            l0++;
        else
            break;
    l0 = l0 + 3;
    scale = 1 / (float) n_max;
    if (radix <= 4)
        for (i = 0; i < N; i += 4)
        {
            /* reversal computation */

            k = _bitr(j) >> l0;
            j++;                                            /* multiple of 4 index */

            x0 = ptr_x0[0];
            x1 = ptr_x0[1];
            x2 = ptr_x0[2];
            x3 = ptr_x0[3];
            x4 = ptr_x0[4];
            x5 = ptr_x0[5];
            x6 = ptr_x0[6];
            x7 = ptr_x0[7];
            ptr_x0 += 8;

            xh0_0 = x0 + x4;
            xh1_0 = x1 + x5;
            xh0_1 = x2 + x6;
            xh1_1 = x3 + x7;

            if (radix == 2)
            {
                xh0_0 = x0;
                xh1_0 = x1;
                xh0_1 = x2;
                xh1_1 = x3;
            }

            yt0 = xh0_0 + xh0_1;
            yt1 = xh1_0 + xh1_1;
            yt4 = xh0_0 - xh0_1;
            yt5 = xh1_0 - xh1_1;

            xl0_0 = x0 - x4;
            xl1_0 = x1 - x5;
            xl0_1 = x2 - x6;
            xl1_1 = x7 - x3;

            if (radix == 2)
            {
                xl0_0 = x4;
                xl1_0 = x5;
                xl1_1 = x6;
                xl0_1 = x7;
            }

            yt2 = xl0_0 + xl1_1;
            yt3 = xl1_0 + xl0_1;
            yt6 = xl0_0 - xl1_1;
            yt7 = xl1_0 - xl0_1;

            y0[k] = yt0 * scale;
            y0[k + 1] = yt1 * scale;
            k += n_max >> 1;
            y0[k] = yt2 * scale;
            y0[k + 1] = yt3 * scale;
            k += n_max >> 1;
            y0[k] = yt4 * scale;
            y0[k + 1] = yt5 * scale;
            k += n_max >> 1;
            y0[k] = yt6 * scale;
            y0[k + 1] = yt7 * scale;
        }

}

/* ======================================================================= */
/*  End of file:  DSPF_sp_ifftSPxSP_cn.c                                   */
/* ----------------------------------------------------------------------- */
/*            Copyright (c) 2009 Texas Instruments, Incorporated.          */
/*                           All Rights Reserved.                          */
/* ======================================================================= */
