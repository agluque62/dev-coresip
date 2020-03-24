#include "fft.h"
#include "processor.h"

#include "DSPF_sp_fftSPxSP.h"
#include "DSPF_sp_fftSPxSP_cn.h"
#include "DSPF_sp_ifftSPxSP_cn.h"

#include <math.h>


static int Initialized = 0;

static float w[2*FFT_SIZE];

static int radix;

static unsigned char brev[64] = {
	0x0, 0x20, 0x10, 0x30, 0x8, 0x28, 0x18, 0x38,
	0x4, 0x24, 0x14, 0x34, 0xc, 0x2c, 0x1c, 0x3c,
	0x2, 0x22, 0x12, 0x32, 0xa, 0x2a, 0x1a, 0x3a,
	0x6, 0x26, 0x16, 0x36, 0xe, 0x2e, 0x1e, 0x3e,
	0x1, 0x21, 0x11, 0x31, 0x9, 0x29, 0x19, 0x39,
	0x5, 0x25, 0x15, 0x35, 0xd, 0x2d, 0x1d, 0x3d,
	0x3, 0x23, 0x13, 0x33, 0xb, 0x2b, 0x1b, 0x3b,
	0x7, 0x27, 0x17, 0x37, 0xf, 0x2f, 0x1f, 0x3f
};


/* generate vector of twiddle factors for optimized algorithm */
static void gen_twiddle(float *w, int n)
{
    int i, j, k;
	const float PI = 3.14159265358979323846f;

    for (j = 1, k = 0; j < n >> 2; j = j << 2)
    {
        for (i = 0; i < n >> 2; i += j)
        {
            w[k +  5] =  (float) sinf((6.0f * PI * (float)i) / (float)n);
            w[k +  4] =  (float) cosf((6.0f * PI * (float)i) / (float)n);

            w[k +  3] =  (float) sinf((4.0f * PI * (float)i) / (float)n);
            w[k +  2] =  (float) cosf((4.0f * PI * (float)i) / (float)n);

            w[k +  1] =  (float) sinf((2.0f * PI * (float)i) / (float)n);
            w[k +  0] =  (float) cosf((2.0f * PI * (float)i) / (float)n);

            k += 6;
        }
    }
}

void fft(complex_num * input, complex_num * output)
{
	//static float x[512];
	//static float y[512];
	//static float win[512];
	//static unsigned char br[512];
	
	//DSPF_sp_fftSPxSP(512,x,win,y,br,4,0,512);
	
	DSPF_sp_fftSPxSP_cn(FFT_SIZE, (float *) input,w, (float *) output,brev,radix,0,FFT_SIZE);
}

void ifft(complex_num * input, complex_num * output)
{
	DSPF_sp_ifftSPxSP_cn(FFT_SIZE, (float *) input,w, (float *) output,brev,radix,0,FFT_SIZE);
}

/****************************************************************************************
* FFT de 128 puntos sobre secuencia real
* void fft128_init(void)
* Inicializa el m�dulo de fft128
* Se puede llamar varias veces, s�lo reservando memoria e inicializando durante la primera
*		llamada a la funci�n
*****************************************************************************************/


void window(complex_num * v, float * window, int length)
{
	int i;

	for (i = 0; i < length; i++)
	{
		v[i].real *= window[i];
		v[i].imaginary *= window[i];
	}
}


void fft_init() {
	if (!Initialized) {	
		Initialized = 1;
		gen_twiddle(w,FFT_SIZE);
		if (FFT_SIZE == 32 ||
				FFT_SIZE == 128 ||
				FFT_SIZE == 512 ||
				FFT_SIZE == 2048 ||
				FFT_SIZE == 8192 ||
				FFT_SIZE == 32768)
			radix = 2;
		else
			radix = 4;
	}
}


