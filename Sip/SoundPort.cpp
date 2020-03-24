/**
 * @file soundport.cpp
 * @brief Gestion de un 'puerto' de sonido en CORESIP.dll
 *
 *	Implementa la clase 'SoundPort', derivada de 'pjmedia_port' para particularizar las funciones
 *	utilizadas en esta aplicacion
 *
 *	@addtogroup CORESIP
 */
/*@{*/
#include "Global.h"
#include "SoundPort.h"
#include "Exceptions.h"
#include "Guard.h"
#include "SipAgent.h"

/**
 * Constructor de la clase 'SoundPort'.
 * @param	type		Tipo de Puerto de Sonido en Nomenclatura SCV @ref CORESIP_SndDevType
 * @param	inDevIndex	En la Estructura de Sonido del PC, número de orden del canal de audio asociado a la entrada (microfono)
 * @param	outDevIndex	En la Estructura de Sonido del PC, número de orden del canal de audio asociado a la salida (casco o altavoz)
 * @return	Nada
 */
SoundPort::SoundPort(CORESIP_SndDevType type, int inDevIndex, int outDevIndex)
{
	/**
	 * Inicializa las Variables de la Clase
	 */
	pj_memset(this, 0, sizeof(SoundPort));
	_RemoteSock = PJ_INVALID_SOCKET;

	/**
	 * Marca los canales y el tipo.
	 */
	InDevIndex = inDevIndex;
	OutDevIndex = outDevIndex;
	_Type = type;

	/**
	 * Inicializa un Pool de Memoria asociado al objeto.
	 */
	_Pool = pjsua_pool_create(NULL, 4096, 512);

	try
	{
		pj_status_t st;

		/**
		 * Inicializa la informacion asociada al Puerto _Port. (PSVP ???). Se autoreferencia 
		 * a la propia clase a traves de _Port.port_data.pdata
		 */
		pjmedia_port_info_init(&_Port.info, &(pj_str("PSVP")), PJMEDIA_PORT_SIGNATURE('P', 'S', 'V', 'P'), 
			SAMPLING_RATE, CHANNEL_COUNT, BITS_PER_SAMPLE, SAMPLES_PER_FRAME);

		_Port.port_data.pdata = this;
		/**
		 * Configura las Callback de _Port.
		 */
		_Port.get_frame = &GetFrame;
		_Port.put_frame = &PutFrame;
		_Port.reset = &Reset;
		_Port.on_destroy = &Dispose;

		/**
		 * Inicializa la informacion asociada al Puerto >this> (SNDP). Se autoreferencia 
		 * a la propia clase a traves de port_data.pdata
		 */
		pjmedia_port_info_init(&info, &(pj_str("SNDP")), PJMEDIA_PORT_SIGNATURE('S', 'N', 'D', 'P'), 
			SipAgent::SndSamplingRate, CHANNEL_COUNT, BITS_PER_SAMPLE, 
			((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000));

		port_data.pdata = this;

		/**
		 * Configura las Callback de <this>.
		 */
		get_frame = &GetSndFrame;
		put_frame = &PutSndFrame;

		/**
		 * Si la frecuencia de muestreo asociada a la tarjeta es diferente de 8 khz, 
		 * crea un Puerto de 'Resampling'.
		 */
		if (SipAgent::SndSamplingRate != SAMPLING_RATE)
		{
			unsigned resample_opt = PJMEDIA_RESAMPLE_DONT_DESTROY_DN;

			if (pjsua_var.media_cfg.quality >= 3 && pjsua_var.media_cfg.quality <= 4)
			{
				resample_opt |= PJMEDIA_RESAMPLE_USE_SMALL_FILTER;
			}
			else if (pjsua_var.media_cfg.quality < 3) 
			{
				resample_opt |= PJMEDIA_RESAMPLE_USE_LINEAR;
			}

			st = pjmedia_resample_port_create(_Pool, this, SAMPLING_RATE, resample_opt, &_ResamplePort);
			PJ_CHECK_STATUS(st, ("ERROR creando _ResamplePort"));
		}

#ifndef PJ_USE_ASIO
#error Hay que compilar con el Flag PJ_USE_ASIO

		st = pjmedia_delay_buf_create(_Pool, NULL, SAMPLING_RATE, SAMPLES_PER_FRAME, CHANNEL_COUNT, 
			SipAgent::DefaultDelayBufPframes * PTIME, 0, &_SndInDbuf);
		PJ_CHECK_STATUS(st, ("ERROR creando _SndInDbuf"));

		st = pjmedia_delay_buf_create(_Pool, NULL, SAMPLING_RATE, SAMPLES_PER_FRAME, CHANNEL_COUNT, 
			SipAgent::DefaultDelayBufPframes * PTIME, 0, &_SndOutDbuf);
		PJ_CHECK_STATUS(st, ("ERROR creando _SndOutDbuf"));

		if (inDevIndex < 0)
		{
			st = pjmedia_snd_port_create_player(_Pool, outDevIndex, SipAgent::SndSamplingRate, CHANNEL_COUNT, 
				((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000), BITS_PER_SAMPLE, 0, &_SndPort);
			PJ_CHECK_STATUS(st, ("ERROR creando snd player", "[outId=%d]", outDevIndex));
		}
		else if (outDevIndex < 0)
		{
			st = pjmedia_snd_port_create_rec(_Pool, inDevIndex, SipAgent::SndSamplingRate, CHANNEL_COUNT,
				((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000), BITS_PER_SAMPLE, 0, &_SndPort);
			PJ_CHECK_STATUS(st, ("ERROR creando snd rec", "[inId=%d]", inDevIndex));
		}
		else
		{
			st = pjmedia_snd_port_create(_Pool, inDevIndex, outDevIndex, SipAgent::SndSamplingRate, CHANNEL_COUNT,
				((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000), BITS_PER_SAMPLE, 0, &_SndPort);
			PJ_CHECK_STATUS(st, ("ERROR creando snd player-rec", "[outId=%d] [inId=%d]", outDevIndex, inDevIndex));
		}

		st = pjmedia_snd_port_connect(_SndPort, &this);
		PJ_CHECK_STATUS(st, ("ERROR adaptando el dispositivo de sonido al mezclador"));

#endif

		/**
		 * Añade el puerto _Port al modulo de Conferencia de pjsua. 
		 * - Retorna un puntero al Slot del puerto en el modulo de conferencia de pjsua.
		 * INFO: http://www.pjsip.org/pjsip/docs/html/group__PJSUA__LIB__MEDIA.htm#ga833528c1019f4ab5c8fb216b4b5f788b
		 */
		st = pjsua_conf_add_port(_Pool, &_Port, &Slot);
		PJ_CHECK_STATUS(st, ("ERROR enlazando dispositivo de sonido al mezclador"));

		st = pj_lock_create_recursive_mutex(_Pool, NULL, &_Lock);
		PJ_CHECK_STATUS(st, ("ERROR creando seccion critica para dispositivo de sonido"));

		/**
		 * Ajusta los niveles de Recepción y Tramsmisión del Slot asociado a _Port.
		 */
		pjsua_conf_adjust_rx_level(Slot, SipAgent::RxLevel);
		pjsua_conf_adjust_tx_level(Slot, SipAgent::TxLevel);

		/**
		 * Configura el Slot como Entrada, Salida o Bidireccional.
		 * INFO: http://www.pjsip.org/pjmedia/docs/html/group__PJMEDIA__CONF.htm#gae2d371bf1e12cb7c93206ba259fba495
		 */
		pjmedia_conf_configure_port(pjsua_var.mconf, Slot, 
			(OutDevIndex >= 0 ? PJMEDIA_PORT_NO_CHANGE : PJMEDIA_PORT_DISABLE), 
			(InDevIndex >= 0 ? PJMEDIA_PORT_NO_CHANGE : PJMEDIA_PORT_DISABLE));
	}
	catch (...)
	{
		Dispose(this);
		throw;
	}
}

/**
 * Destructor de la Clase SoundPort.
 * @return	Nada
 */
SoundPort::~SoundPort()
{
	pjsua_conf_remove_port(Slot);
	Dispose(this);
}

/**
 * Remote.	
 * @param	on		...
 * @param	id		Puntero a ...
 * @param	ip		Puntero a ...
 * @param	port	...
 * @return	Nada
 */
void SoundPort::Remote(bool on, const char * id, const char * ip, unsigned port)
{
	static int rPayloadSize = sizeof(_RemotePayload);
	Guard lock(_Lock);

	if (_RemoteSock != PJ_INVALID_SOCKET)
	{
		pj_sock_close(_RemoteSock);
		_RemoteSock = PJ_INVALID_SOCKET;
	}

	if (on)
	{
		pj_ansi_strcpy(_RemotePayload.SrcId, id);
		_RemotePayload.SrcType = _Type;

		pj_status_t st = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &_RemoteSock);
		PJ_CHECK_STATUS(st, ("ERROR creando socket para el envio de radio por unicast"));

		pj_sockaddr_in_init(&_RemoteTo, &(pj_str(const_cast<char*>(ip))), (pj_uint16_t)port);
	}
}

#ifdef PJ_USE_ASIO

/**
 * GetFrame.	...
 * Solo si se usa la Interfaz ASIO
 * @param	port	Puntero 'pjmedia_port' a...
 * @param	frame	Puntero 'pjmedia_frame' a...
 * @return	pj_status_t	Resultado de la operacion.
 */
pj_status_t SoundPort::GetFrame(pjmedia_port * port, pjmedia_frame * frame)
{
	pj_assert(frame->size == SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8));

	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);
	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	memcpy(frame->buf, (SipAgent::SndSamplingRate != SAMPLING_RATE ? pThis->_SndIn : pThis->_SndInBuf), frame->size);

	return PJ_SUCCESS;
}

/**
 * PutFrame.	...
 * Solo si se utiliza la interfaz ASIO
 * @param	port		Puntero 'pjmedia_port' a...
 * @param	frame		Puntero 'pjmedia_frame' a...
 * @return	pj_status_t	Resultado de la operacion.
 */
pj_status_t SoundPort::PutFrame(pjmedia_port * port, const pjmedia_frame * frame)
{
	pj_assert((frame->type != PJMEDIA_FRAME_TYPE_AUDIO) || 
		(frame->size == SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8)));

	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);

	if (SipAgent::SndSamplingRate != SAMPLING_RATE)
	{
		return pjmedia_port_put_frame(pThis->_ResamplePort, frame);
	}
	else
	{
		return PutSndFrame(port, frame);
	}
}

/**
 * Reset.		...
 * Solo si se utiliza la interfaz ASIO
 * @param	port		Puntero 'pjmedia_port' a...
 * @return	pj_status_t	Resultado de la operacion.
 */
pj_status_t SoundPort::Reset(pjmedia_port * port)
{
	return PJ_SUCCESS;
}

/**
 * GetSndFrame.	...
 * Solo si se utiliza la interfaz ASIO
 * @param	port		Puntero 'pjmedia_port' a...
 * @param	frame		Puntero 'pjmedia_frame' a...
 * @return	pj_status_t	Resultado de la operacion.
 */
pj_status_t SoundPort::GetSndFrame(pjmedia_port * port, pjmedia_frame * frame)
{
	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);

	pj_assert(frame->size == ((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000) * (BITS_PER_SAMPLE / 8));

	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	memcpy(frame->buf, pThis->_SndInBuf, frame->size);

	return PJ_SUCCESS;
}

/**
 * PutSndFrame.	...
 * Solo si se utiliza la interfaz ASIO
 * @param	port		Puntero 'pjmedia_port' a...
 * @param	frame		Puntero 'pjmedia_frame' a...
 * @return	pj_status_t	Resultado de la operacion.
 */
pj_status_t SoundPort::PutSndFrame(pjmedia_port * port, const pjmedia_frame * frame)
{
	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);

	if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO)
	{
		pj_assert(frame->size == ((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000) * (BITS_PER_SAMPLE / 8));
		memcpy(pThis->_SndOutBuf, frame->buf, frame->size);
	}
	else
	{
		memset(pThis->_SndOutBuf, 0, ((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000) * (BITS_PER_SAMPLE / 8));
	}

	return PJ_SUCCESS;
}

/**
 * SetInBuf	...
 * Solo si se utiliza la interfaz ASIO
 * @param	frame	Puntero 'pjmedia_frame'  a...
 * @param	offset	...
 * @return	Nada
 */
void SoundPort::SetInBuf(pjmedia_frame * frame, int offset) 
{ 
	_SndInBuf = (char*)frame->buf + offset; 

	if (SipAgent::SndSamplingRate != SAMPLING_RATE)
	{
		pjmedia_frame fr = { PJMEDIA_FRAME_TYPE_NONE };
		fr.buf = _SndIn;
		fr.timestamp.u64 = frame->timestamp.u64;

		pjmedia_port_get_frame(_ResamplePort, &fr);
	}

	if (_RemoteSock != PJ_INVALID_SOCKET)
	{
		Guard lock(_Lock);

		if (_RemoteSock != PJ_INVALID_SOCKET)
		{
			static pj_ssize_t size = sizeof(RemotePayload);

			_RemotePayload.Size = SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8);
			memcpy(_RemotePayload.Data, (SipAgent::SndSamplingRate != SAMPLING_RATE ? _SndIn : _SndInBuf), SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8));

			pj_sock_sendto(_RemoteSock, &_RemotePayload, &size, 0, &_RemoteTo, sizeof(_RemoteTo));
		}
	}
}

/**
 * SetOutBuffer.	...
 * Solo si se utiliza la interfaz ASIO
 * @param	frame		Puntero 'pjmedia_frame' a...
 * @return	Nada
 */
void SoundPort::SetOutBuf(pjmedia_frame * frame, int offset) 
{ 
	_SndOutBuf = (char*)frame->buf + offset; 
}

#else
#error Hay que compilar con el Flag PJ_USE_ASIO

pj_status_t SoundPort::GetFrame(pjmedia_port * port, pjmedia_frame * frame)
{
	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);

	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	return pjmedia_delay_buf_get(pThis->_SndInDbuf, (pj_int16_t*)frame->buf);
}

pj_status_t SoundPort::PutFrame(pjmedia_port * port, const pjmedia_frame * frame)
{
	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);

	if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO)
	{
		pjmedia_delay_buf_put(pThis->_SndOutDbuf, (pj_int16_t*)frame->buf);
	}
	else
	{
		static pj_int16_t zeroes[SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8)] = { 0 };
		pjmedia_delay_buf_put(pThis->_SndOutDbuf, zeroes);
	}

	return PJ_SUCCESS;
}

pj_status_t SoundPort::Reset(pjmedia_port * port)
{
	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);
	pjmedia_delay_buf_reset(pThis->_SndInDbuf);

	return PJ_SUCCESS;
}

pj_status_t SoundPort::GetSndFrame(pjmedia_port * port, pjmedia_frame * frame)
{
	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);

	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	if (SipAgent::SndSamplingRate != SAMPLING_RATE)
	{
		return pjmedia_port_get_frame(pThis->_ResamplePort, frame);
	}
	else
	{
		return pjmedia_delay_buf_get(pThis->_SndOutDbuf, (pj_int16_t*)frame->buf);
	}
}

pj_status_t SoundPort::PutSndFrame(pjmedia_port * port, const pjmedia_frame * frame)
{
	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);

	pThis->_SndInBuf = frame->buf; 

	if (SipAgent::SndSamplingRate != SAMPLING_RATE)
	{
		pjmedia_frame fr = { PJMEDIA_FRAME_TYPE_NONE };
		fr.buf = pThis->_SndIn;
		fr.timestamp.u64 = frame->timestamp.u64;

		pjmedia_port_get_frame(pThis->_ResamplePort, &fr);
	}

	if (pThis->_RemoteSock != PJ_INVALID_SOCKET)
	{
		Guard lock(pThis->_Lock);

		if (pThis->_RemoteSock != PJ_INVALID_SOCKET)
		{
			static pj_ssize_t size = sizeof(RemotePayload);

			pThis->_RemotePayload.Size = SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8);
			memcpy(pThis->_RemotePayload.Data, (SipAgent::SndSamplingRate != SAMPLING_RATE ? pThis->_SndIn : pThis->_SndInBuf), SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8));

			pj_sock_sendto(pThis->_RemoteSock, &pThis->_RemotePayload, &size, 0, &pThis->_RemoteTo, sizeof(pThis->_RemoteTo));
		}
	}

	return pjmedia_delay_buf_put(pThis->_SndInDbuf, (pj_int16_t*)(SipAgent::SndSamplingRate != SAMPLING_RATE ? pThis->_SndIn : pThis->_SndInBuf));
}

#endif

/**
 * Dispose.		...
 * @param	port		Puntero 'pjmedia_port' a...
 * @return	pj_status_t	Resultado de la operacion.
 */
pj_status_t SoundPort::Dispose(pjmedia_port * port) 
{
	SoundPort * pThis = reinterpret_cast<SoundPort*>(port->port_data.pdata);

	if (pThis)
	{
		pThis->port_data.pdata = NULL;

		if (pThis->_ResamplePort)
		{
			pjmedia_port_destroy(pThis->_ResamplePort);
		}

#ifndef PJ_USE_ASIO
#error Hay que compilar con el Flag PJ_USE_ASIO

		if (pThis->_SndPort)
		{
			pjmedia_snd_port_destroy(pThis->_SndPort);
		}
		if (pThis->_SndOutDbuf)
		{
			pjmedia_delay_buf_destroy(pThis->_SndOutDbuf);
		}
		if (pThis->_SndInDbuf)
		{
			pjmedia_delay_buf_destroy(pThis->_SndInDbuf);
		}
#endif

		if (pThis->_RemoteSock != PJ_INVALID_SOCKET)
		{
			pj_sock_close(pThis->_RemoteSock);
		}
		if (pThis->_Lock)
		{
			pj_lock_destroy(pThis->_Lock);
		}
		if (pThis->_Pool)
		{
			pj_pool_release(pThis->_Pool);
		}
	}

	return PJ_SUCCESS;
}

/*@}*/
