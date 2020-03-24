#ifndef PROCESSOR_H_
#define PROCESSOR_H_

#include "fft.h"

//#define DEBUG_BSS

#define PROCESSOR_BLOCK_SIZE (FFT_SIZE)
#define PROCESSOR_BLOCK_OVERLAP (FFT_SIZE/2)
#define PROCESSOR_HANN_WINDOW hann512

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

	/*
	 * Estructura para el almacenamiento de datos intermedios para una señal.
	 */
	typedef struct processor_data
	{
		int initiated;
		int instance_id;

		complex_num cBlock0[PROCESSOR_BLOCK_SIZE];
		complex_num cBlock1[PROCESSOR_BLOCK_SIZE];

		complex_num * cBlock;
		int cBlockPos;

		complex_num lastCepsFreq;
		complex_num lastCepsFreqSt;
		complex_num quality;

		char fproc;	// JJA Flag para indicar que se ha realizado el procesamiento interno (process_internal)

		float fDebug;
		/** AGL.. */
		float sample_max;
	} processor_data_t;

	/*
	 * Inicialización de la estrucutra de datos.
	 * Las primeras instance_id llamadas a process(processor_data_t, float, int) son descartadas para repartir temporalmente la carga de la CPU entre las diferentes instancias
	 *
	 * instance: puntero a la estructura a inicializar.
	 * instance_id: identificador numérico de la estructura
	 */
	void processor_init(processor_data_t * instance, int instance_id);

	/*
	 * Procesa el nuevo bloque apuntado por v de tamaño count
	 *
	 * instance: puntero a la estructura de datos asignada a la señal del bloque v
	 * v: puntero al inicio de las nuevas muestras no procesadas
	 * count: número de muestras a procesar
	 */
	int process(processor_data_t * instance, float * v, int count);

	/*
	 * Indicador de calidad actual en número entero (0 - 5)
	 *
	 * data: puntero a la estructura de datos asignada a la señal a evaluar
	 */
	int quality_indicator(processor_data_t * data);

	/*
	 * Indicador de calidad actual en número flotante (0.0f - 5.0f)
	 *
	 * data: puntero a la estructura de datos asignada a la señal a evaluar
	 */
	float quality_indicator_float(processor_data_t * data);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif /* PROCESSOR_H_ */
