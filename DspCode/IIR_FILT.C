
/**
 * \brief		Implementación de un filtro IIR
 * \param x		puntero a las muestras de entrada
 * \param y 	puntero a las muestras ya filtradas
 * \param x_d	puntero a las muestras de entrada retrasadas (trama anterior)
 * \param y_d	puntero a las muestras filtradas retrasadas (trama anterior)
 * \param b		coeficientes del numerador
 * \param a		coeficientes del denominador
 * \param N		orden del filtro
 * \param numY	número de muestras de salida
 
 IIR de orden 2 utilizado para:
 - Notch a 2280Hz en los protocolos de LCE y R2
 - Notch tono de invitación a marcar. Decodificación DTMF
 
 IIR de orden 4 que usamos para:
 - Notch a 2400Hz y 2600Hz utilizado en el protocolo N5.
 - Filtro de diezmado usado con BL para calcular el espectro en tiempo real. La relación de diezmado es de 20/1
 (se pasa de un ancho de banda de 4000Hz a 200Hz)
 - Paso alto eliptico a 1020Hz para detección FSK en telefonía
 - Paso alto (fc=300Hz) para eliminar ruido baja frecuencia por el micro del handset
 
 IIR de orden 7:
 - Paso Alto utilizado para eliminar el tono de invitación a marcar generado por la centralita
 y el elevado ruido de BF que hay en las líneas.	Se utiliza en la decodificación de DTMF para poder
 seguir las variaciones de nivel de los dígitos MF que van superpuestos a dicho tono.	
 - Paso Bajo utilizado para eliminar tonos superiores a 2700Hz
 - Filtros diezmado e interpolación de la entrada/salida
 */
void iir(float *x, float *y, float *b, float *a, float *x_d, float *y_d, int N, int numY)
{
	int i,j;
	float *X, *Y;

	y[0] = b[0]*x[0];
	for (j = 1;j<=N;j++)
		y[0] += b[j]*x_d[N-j] - a[j]*y_d[N-j];
		 
	for (i=1;i<numY;i++)
	{
		y[i] = b[0]*x[i];
		X=&x[i-1];
		Y=&y[i-1];
		for (j = 1;j<=N;j++)
		{
			y[i] += b[j] * *X-- - a[j]* *Y--;
			if (i==j){
				X=&x_d[N-1];
				Y=&y_d[N-1];
			}
		}
	}
	for (i=0,j=N;i<N;i++,j--)
	{
		x_d[i] = x[numY-j];  
		y_d[i] = y[numY-j];
	}
}


/**
 * \brief		Implementación de un filtro FIR
 * \param x		puntero a las muestras de entrada
 * \param y 	puntero a las muestras ya filtradas
 * \param h		coeficientes del filtro
 * \param numH	número de coeficientes
 * \param numY	número de muestras de salida
 */
void fir(float *x, float *h, float *y, int numH, int numY)
{
   int i, j;
   float sum;

   for(j=0; j < numY; j++)
   {
      sum = 0;
      for(i=0; i < numH; i++)
      {
		sum += x[i+j] * h[i];
      }
      y[j] = sum;
   }
}
