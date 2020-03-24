#ifndef __IIR_FILT
#define __IIR_FILT

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void iir(float *x, float *y, float *b, float *a, float *x_d, float *y_d, int N, int numY);
void fir(float *x, float *h, float *y, int numH, int numY);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif