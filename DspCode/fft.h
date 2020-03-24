#ifndef __FFT
#define __FFT

#define FFT_SIZE (512)

#include "complexnums.h"

/****************************************************************************************
* FFT de FFT_SIZE puntos sobre secuencia real
* void fft128(float * input, float * output)
* Recoge 128 muestras complejas punto flotante
* Devuelve 128 muestras frecuenciales complejas
* No es inplace
****************************************************************************************/
void fft(complex_num * input,complex_num * output);
//void fft(void);

/****************************************************************************************
* IFFT de FFT_SIZE puntos sobre secuencia compleja
* void ifft128(float * input, float * output)
* Recoge FFT_SIZE muestras complejas punto flotante ALINEADAS A PALABRAS PARES
* Devuelve los FFT_SIZE puntos temporales complejos (mitad de tama�o que la entrada)
* No es inplace
* El buffer de entrada debe tener [FFT_SIZE*2 (datos) + 16 (colchón)] palabras
* Los buffers tienen que estar alineados a 2
****************************************************************************************/
void ifft(complex_num * input, complex_num * output);

/****************************************************************************************
* FFT de 128 puntos sobre secuencia real
* void fft128_init(void)
* Inicializa el m�dulo de fft128
* Se puede llamar varias veces, s�lo reservando memoria e inicializando durante la primera
*		llamada a la funci�n
*****************************************************************************************/
void fft_init();

void window(complex_num * v, float * window, int size);

#endif
