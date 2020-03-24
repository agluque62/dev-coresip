/**
 * @file soundport.h
 * @brief Gestion de un 'puerto' de sonido en CORESIP.dll
 *
 *	Implementa la clase 'SoundPort', derivada de 'pjmedia_port' para encapsular las funciones utilizadas 
 *	para interfasarse a una tajeta de sonido.
 *
 *	@addtogroup CORESIP
 */
/*@{*/
#ifndef __CORESIP_SOUNDPORT_H__
#define __CORESIP_SOUNDPORT_H__

/**
 * RemotePayload: ...
 */
struct RemotePayload
{
	char SrcId[CORESIP_MAX_USER_ID_LENGTH + 1];
	CORESIP_SndDevType SrcType;
	unsigned Size;
	char Data[SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8)];
};

/**
 * SoundPort: Encapsula y particulariza las funciones de 'pjmedia_port' en esta aplicación.
 *
 * Existen tres objetos 'pjmedia_port':
 *	- El heredado por 'SoundPort' de pjmedia_port.
 *	- El referenciado por la variable _Port.
 *	- El referenciado por el Puntero _ResamplePort.
 */
class SoundPort : public pjmedia_port
{
public:
	/**
	 * Puerto de Conferencia.
	 */
	pjsua_conf_port_id Slot;
	/**
	 * Indice del Canal de Entrada (en el esquema general de audio del PC).
	 */
	int InDevIndex;
	/**
	 * Indice el Canl de Salida (en el esquema general de audio del PC).
	 */
	int OutDevIndex;

	/**
	 * Tipo de Puerto de Audio (Ejecutivo, Ayudante, etc..)
	 */
	CORESIP_SndDevType _Type;

public:
	SoundPort(CORESIP_SndDevType type, int inDevIndex, int outDevIndex);
	SoundPort() {}
	~SoundPort();

	void Remote(bool on, const char * id, const char * ip, unsigned port);

#ifdef PJ_USE_ASIO
	void SetInBuf(pjmedia_frame * frame, int offset);
	void SetOutBuf(pjmedia_frame * frame, int offset);
#endif

private:

	pj_uint16_t contador_muestra;
	pj_uint16_t contador_paquete;
	
	/**
	 * Pool de Memoria asociada al objeto.
	 */
	pj_pool_t * _Pool;
	/**
	 * Semafono de acceso asociado a los datos del objeto.
	 */
	pj_lock_t * _Lock;
	/**
	 * ???
	 */
	pj_sock_t _RemoteSock;
	/**
	 * ???
	 */
	pj_sockaddr_in _RemoteTo;
	/**
	 * ???
	 */
	RemotePayload _RemotePayload;
	/**
	 * Objeto Puerto PJMEDIA...
	 */
	pjmedia_port _Port;
	/**
	 * ???
	 */
	pjmedia_port * _ResamplePort;
	/**
	 * ???
	 */
	char _SndIn[SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8)];

#ifdef PJ_USE_ASIO
	/**
	 * Con ASIO Buffer de Entrada ( ¿ desde el microfono ?).
	 */
	void * _SndInBuf;
	/**
	 * Con ASIO Buffer de Salida ( ¿ hacia cascos o altavoz ?).
	 */
	void * _SndOutBuf;
#else
	pjmedia_snd_port * _SndPort;
	pjmedia_delay_buf * _SndInDbuf;
	pjmedia_delay_buf * _SndOutDbuf;
#endif

private:
	static pj_status_t GetFrame(pjmedia_port * port, pjmedia_frame * frame);
	static pj_status_t PutFrame(pjmedia_port * port, const pjmedia_frame * frame);
	static pj_status_t Reset(pjmedia_port * port);
	static pj_status_t Dispose(pjmedia_port * port);

	static pj_status_t GetSndFrame(pjmedia_port * port, pjmedia_frame * frame);
	static pj_status_t PutSndFrame(pjmedia_port * port, const pjmedia_frame * frame);
};

#endif
/*@}*/