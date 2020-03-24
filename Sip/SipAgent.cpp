/**
 * @file sipagent.cpp
 * @brief Gestion de un Agente SIP en CORESIP.dll
 *
 *	Implementa la clase 'SipAgent', que encapsula las funciones 'pjsip' correspondientes al Agente.
 *
 *	Utiliza como base la libreria 'pjsua'
 *
 *	@addtogroup CORESIP
 */
/*@{*/
#include "Global.h"
#include "SipAgent.h"
#include "Exceptions.h"
#include "SipCall.h"
#include "Guard.h"
#ifdef PJ_USE_ASIO
#include <pa_asio.h>
#endif
#include "wg67subscription.h"

/**
 * OnMemAllocFail: Rutina de tratamiento de las excepciones de Memoria.
 * - Genera una excepcion global.
 * @param	pool	Puntero 'pj_pool' al objeto que ha generado el error.
 * @param	size	Tamaño 'pj_size': del Objeto.
 * @return	nada
 */
static void OnMemAllocFail(pj_pool_t *pool, pj_size_t size)
{
	throw PJLibException(__FILE__, PJ_ENOMEM).Msg("ERROR reservando memoria");
}

/**
 * coresip_mod: Estructura de configuracion del Módulo.
 * - Informacion: http://www.pjsip.org/pjsip/docs/html/structpjsip__module.htm
 */
static pjsip_module coresip_mod = 
{
	NULL, NULL,									/* prev, next.			*/
	{ "mod-coresip", 11 },					/* Name.				*/
	-1,											/* Id				*/
	PJSIP_MOD_PRIORITY_APPLICATION + 1,	/* Priority			*/
	NULL,											/* load()				*/
	NULL,											/* start()				*/
	NULL,											/* stop()				*/
	NULL,											/* unload()				*/
	NULL,											/* on_rx_request()			*/
	&SipAgent::OnRxResponse,				/* on_rx_response()			*/
	NULL,											/* on_tx_request.			*/
	NULL,											/* on_tx_response()			*/
	NULL,											/* on_tsx_state()			*/
};

/**
 * SipAgent::Cb: Bloque de Callbacks asociados al agente 'pjsua'
 */
CORESIP_Callbacks SipAgent::Cb;
/**
 *	SipAgent::EnableMonitoring: ...
 */
bool SipAgent::EnableMonitoring = false;
/**
 *	SipAgent::SndSamplingRate: ...
 */
unsigned SipAgent::SndSamplingRate = SND_SAMPLING_RATE;
/**
 *	SipAgent::DefaultDelayBufPframes: ...
 */
unsigned SipAgent::DefaultDelayBufPframes = DELAY_BUF_PFRAMES;
/**
 *	SipAgent::RxLevel: ...
 */
float SipAgent::RxLevel = 1.0;
/**
 *	SipAgent::TxLevel: ...
 */
float SipAgent::TxLevel = 1.0;
/**
 *	SipAgent::_Lock: Puntero a ...
 */
pj_lock_t * SipAgent::_Lock = NULL;
/**
 *	SipAgent::_KeepAlivePeriod: ...
 */
unsigned SipAgent::_KeepAlivePeriod = 200;
/**
 *	SipAgent::_KeepAliveMultiplier: ...
 */
unsigned SipAgent::_KeepAliveMultiplier = 10;
/**
 *	SipAgent::_SndPorts: Lista de Punteros a los Dispositivos de Sonido.
 */
SoundPort * SipAgent::_SndPorts[CORESIP_MAX_SOUND_DEVICES];
/**
 *	SipAgent::_WavPlayers: Lista de Punteros a los Reproductores Wav.
 */
WavPlayer * SipAgent::_WavPlayers[CORESIP_MAX_WAV_PLAYERS];
/**
 *	SipAgent::_WavRecorders: Lista de Punteros a los Grabadores Wav.
 */
WavRecorder * SipAgent::_WavRecorders[CORESIP_MAX_WAV_RECORDERS];
/**
 *	SipAgent::_RdRxPorts: Lista de Punteros a los Puertos de Recepcion Radio.
 */
RdRxPort * SipAgent::_RdRxPorts[CORESIP_MAX_RDRX_PORTS];
/**
 *	SipAgent::_SndRxPorts: Lista de Punteros a los Puertos de Recepcion.
 */
SoundRxPort * SipAgent::_SndRxPorts[CORESIP_MAX_SOUND_RX_PORTS];
/**
 *	SipAgent::_RecordPort: Punteros a los Puertos de Grabacion Tel y Rad.
 */
RecordPort * SipAgent::_RecordPortTel = NULL;
RecordPort * SipAgent::_RecordPortRad = NULL;

/**
 *	SipAgent::_SndRxIds: Mapa de acceso por nombre (string) a los punteros a los dispositos de sonido.
 */
std::map<std::string, SoundRxPort*> SipAgent::_SndRxIds;
/**
 *	SipAgent::_Sock: pj_sock. Socket asociado al agente.
 */
pj_sock_t SipAgent::_Sock = PJ_INVALID_SOCKET;
/**
 *	SipAgent::_RemoteSock: Puntero a 'pj_activesock_t ...
 */
pj_activesock_t * SipAgent::_RemoteSock = NULL;
/**
 *	SipAgent::_NumInChannels: ...
 */
int SipAgent::_NumInChannels = 0;
/**
 *	SipAgent::_NumOutChannels: ...
 */
int SipAgent::_NumOutChannels = 0;
/**
 *	SipAgent::_InChannels: Tabla ...
 */
int SipAgent::_InChannels[10 * CORESIP_MAX_SOUND_DEVICES];
/**
 *	SipAgent::_OutChannels: Tabla ...
 */
int SipAgent::_OutChannels[10 * CORESIP_MAX_SOUND_DEVICES];
/**
 *	SipAgent::_SndDev: Puntero a pjmedia_aud_stream ...
 */
pjmedia_aud_stream * SipAgent::_SndDev = NULL;
/**
 *	SipAgent::_ConfMasterPort: Puntero a pjmedia_port ...
 */
pjmedia_port * SipAgent::_ConfMasterPort = NULL;

/**
 *	AGL 21131121. Valores de Configuración del Cancelador.
 */
unsigned SipAgent::EchoTail = 100;
unsigned SipAgent::EchoLatency = 0;
/* FM */

char SipAgent::uaIpAdd[32];
unsigned int SipAgent::uaPort = 0;

/**
 * Init. Inicializacion de la clase:
 * - Incializa las Variables de la Clase, tanto locales como de librería.
 * - Crea el Agente SIP.
 * @param	cfg		Puntero a la configuracion local.
 * @return	Nada
 */
void SipAgent::Init(const CORESIP_Config * cfg)
{
	/**
	 * Carga la configuracion Recibida en la configuracion local de la clase.
	 */
	Cb = cfg->Cb;
	DefaultDelayBufPframes = cfg->DefaultDelayBufPframes;
	SndSamplingRate = cfg->SndSamplingRate;
	RxLevel = cfg->RxLevel;
	TxLevel = cfg->TxLevel;
	gJBufPframes = cfg->DefaultJBufPframes;

	/* AGL 20131121. Variables para la configuracion del Cancelador de Echo */
	EchoTail = cfg->EchoTail;
	EchoLatency = cfg->EchoLatency;

	/*
	char msg[64];
	sprintf(msg,"EchoTail=%d, EchoLatency=%d.", EchoTail, EchoLatency);
	Cb.LogCb(2,msg,strlen(msg));
	*/
	/* FM */


	/**
	 * Callback para el tratamiento de los Errores de Memoria.
	 */
	pj_pool_factory_default_policy.callback = &OnMemAllocFail;

	/**
	 * Configura otra serie de Callbacks
	 */
	pj_app_cbs.on_stream_rtp = &SipCall::OnRdRtp;
	pj_app_cbs.on_stream_rtp_ext_info_changed = &SipCall::OnRdInfoChanged;
	pj_app_cbs.on_stream_ka_timeout = &SipCall::OnKaTimeout;
	pj_app_cbs.on_create_sdp = &SipCall::OnCreateSdp;
	
#ifdef _ED137_
	// Heredadas de coresip PlugTest FAA 05/2011
	//pj_app_cbs.on_create_uac_tsx = &OnCreateUacTsx;
	//pjsip_app_cbs.on_create_sdp = &OnCreateSdp;
	pj_app_cbs.on_negociate_sdp = &SipCall::OnNegociateSdp;
	//pj_app_cbs.on_creating_transfer_call = &OnCreatingTransferCall;
#endif

	/**
	 * Crea el agente SIP.
	 */
	pj_status_t st = pjsua_create();
	PJ_CHECK_STATUS(st, ("ERROR creando agente Sip"));

	try
	{
		/**
		 * Bloques de Configuracion de 'pjsua'.
		 * - Declaracion e
		 * - Inicializacion con los valores por defecto
		 */
		pjsua_config uaCfg;
		pjsua_logging_config logCfg;
		pjsua_media_config mediaCfg;
		pjsua_transport_config transportCfg;
		pjsua_config_default(&uaCfg);
		pjsua_logging_config_default(&logCfg);
		pjsua_media_config_default(&mediaCfg);
		pjsua_transport_config_default(&transportCfg);

		/**
		 * Configuracion del Bloque de Configuracion de 'pjsua'.
		 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__config.htm
		 */
		uaCfg.max_calls = 240;
		uaCfg.user_agent = pj_str("U5K-UA/1.0.0");		// AGL. pj_str("CD40/0.0.0");
		uaCfg.cb.on_call_state = &SipCall::OnStateChanged;						// Trata los Cambios de estado de una Llamada.
		uaCfg.cb.on_incoming_call = &SipCall::OnIncommingCall;					// Trata la Notificacion de Llamada Entrante.
		uaCfg.cb.on_call_media_state = &SipCall::OnMediaStateChanged;			// Trata la conexion Desconexion de la Media a un dispositivo.
		uaCfg.cb.on_call_transfer_request = &SipCall::OnTransferRequest;		// Trata la notificación de transferencia de llamada.
		uaCfg.cb.on_call_transfer_status = &SipCall::OnTransferStatusChanged;	// Trata los cambios de estado de una llamada previamente transferidad
		uaCfg.cb.on_call_tsx_state = &SipCall::OnTsxStateChanged;				// Trata cambios genericos en el estado de una llamada.
		uaCfg.cb.on_stream_created = &SipCall::OnStreamCreated;					// Gestiona la creación de los stream y su conexion a los puertos de conferencia
		uaCfg.nat_type_in_sdp = 0;												// No information will be added in SDP, and parsing is disabled.

		/** AGL */
		uaCfg.require_timer=PJ_FALSE;
		uaCfg.require_100rel=PJ_FALSE;

		/**
		 * Configuracion del Bloque de 'LOG' de 'pjsua'.
		 */
		logCfg.msg_logging = cfg->LogLevel >= 4;
		logCfg.level = cfg->LogLevel;
		logCfg.console_level = cfg->LogLevel;

		if (cfg->Cb.LogCb)
		{
			logCfg.decor = 0;
			logCfg.cb = Cb.LogCb;
		}
		else
		{
			logCfg.log_filename = pj_str(const_cast<char*>("./logs/coresip.txt"));
		}

		/**
		 * Configuracion del Bloque MEDIA de 'pjsua'.
		 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__media__config.htm#a2c95e5ce554bbee9cc60d0328f508658
		 */
		mediaCfg.clock_rate = SAMPLING_RATE;
		mediaCfg.audio_frame_ptime = PTIME;
		mediaCfg.channel_count = CHANNEL_COUNT;
		mediaCfg.max_media_ports = uaCfg.max_calls + 10;
		mediaCfg.no_vad = PJ_TRUE;
		mediaCfg.snd_auto_close_time = -1;

		/**
		 * PJSIP run-time configurations/settings.
		 * http://www.pjsip.org/pjsip/docs/html/structpjsip__cfg__t.htm
		 */
		pjsip_sip_cfg_var.tsx.t1 = cfg->TsxTout / 2;
		pjsip_sip_cfg_var.tsx.tsx_tout = cfg->TsxTout;
		pjsip_sip_cfg_var.tsx.inv_proceeding_ia_tout = cfg->InvProceedingIaTout;
		pjsip_sip_cfg_var.tsx.inv_proceeding_monitoring_tout = cfg->InvProceedingMonitoringTout;
		pjsip_sip_cfg_var.tsx.inv_proceeding_dia_tout = cfg->InvProceedingDiaTout;
		pjsip_sip_cfg_var.tsx.inv_proceeding_rd_tout = cfg->InvProceedingRdTout;

		/**
		 * Inicializa 'pjsua', con la configuracion establecida.
		 */
		st = pjsua_init(&uaCfg, &logCfg, &mediaCfg);
		PJ_CHECK_STATUS(st, ("ERROR inicializando Sip"));

		//st = pjsip_wg67_init_module(pjsua_get_pjsip_endpt());
		//PJ_CHECK_STATUS(st, ("ERROR inicialiando modulo WG67 KEY-IN"));

		/**
		 * Inicializacion del módulo de conferencia.
		 */
		st = pjsip_conf_init_module(pjsua_get_pjsip_endpt());
		PJ_CHECK_STATUS(st, ("ERROR inicialiando modulo conference"));

		if (cfg->DefaultCodec[0])
		{
			pjsua_codec_set_priority(&(pj_str(const_cast<char*>(cfg->DefaultCodec))), PJMEDIA_CODEC_PRIO_HIGHEST);
		}

		/**
		 * Registra el modulo en la libreria.
		 */
		st = pjsip_endpt_register_module(pjsua_var.endpt, &coresip_mod);
		PJ_CHECK_STATUS(st, ("ERROR registrando modulo coresip"));

		/**
		 * Inicializa la configuracion del Bloque de Transporte.
		 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__transport__config.htm
		 */
		transportCfg.port = cfg->Port;
		transportCfg.bound_addr = pj_str(const_cast<char*>(cfg->IpAddress));
		strcpy(SipAgent::uaIpAdd, cfg->IpAddress);
		SipAgent::uaPort = cfg->Port;
		transportCfg.options = PJMEDIA_UDP_NO_SRC_ADDR_CHECKING;

		/**
		 * Crea el Transporte para SIP. UDP en puerto Base.
		 */
		st = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transportCfg, NULL);
		PJ_CHECK_STATUS(st, ("ERROR creando puertos UDP para Sip", "(%s:%d)", cfg->IpAddress, cfg->Port));

		/**
		 * Crea el Transporte para RTP. UDP en puerto Base+2
		 */
		transportCfg.port += 2;
		st = pjsua_media_transports_create(&transportCfg);
		PJ_CHECK_STATUS(st, ("ERROR creando puertos UDP para RTP", "(%s:%d)", cfg->IpAddress, transportCfg.port));

		/**
		 * Crea un control de acceso a la memoria del agente.
		 */
		st = pj_lock_create_recursive_mutex(pjsua_var.pool, NULL, &_Lock);
		PJ_CHECK_STATUS(st, ("ERROR creando seccion critica"));

		/** 
		* Crea módulo de suscripción WG67
		**/
		//WG67Subscription::Init(NULL, cfg);

		/**
			* Se crea el puerto pjmedia para la grabacion
		 */
		if (cfg->RecordingEd137)
		{
			_RecordPortTel = new RecordPort(RecordPort::TEL_RESOURCE, cfg->IpAddress, "127.0.0.1", 65003, cfg->HostId);		
			_RecordPortRad = new RecordPort(RecordPort::RAD_RESOURCE, cfg->IpAddress, "127.0.0.1", 65003, cfg->HostId);						
		}
		else
		{
			_RecordPortTel = NULL;
			_RecordPortRad = NULL;
		}
	}
	catch (...)
	{
		pjsua_destroy();
		throw;
	}
}

/**
 * Start. Rutina de Arranque del módulo. Esta rutina, está realizada para ser utilizada con el Driver ASIO.
 * @return	Nada
 */
void SipAgent::Start()
{

#ifdef PJ_USE_ASIO

	/**
	 * Solo si hay Canales (puertos) de audio de Entrada y/o salida.
	 */
	if ((_NumInChannels + _NumOutChannels) > 0)
	{
		/**
		 * Desconecta el Puerto por defecto que asigna la librería a los modulos.
		 * INFO: http://www.pjsip.org/pjsip/docs/html/group__PJSUA__LIB__MEDIA.htm#ga4b6ffc203b8799f08f072d0921777161
		 */
		_ConfMasterPort = pjsua_set_no_snd_dev();
		/** 
		 * Desconecta los puertos, también del módulo de Conferencias.
		 */
		pjmedia_conf_configure_port(pjsua_var.mconf, 0, PJMEDIA_PORT_DISABLE, PJMEDIA_PORT_DISABLE);

		/**
		 * Para configurar canales 'pjmedia' que utilicen ASIO.
		 */
		pjmedia_aud_param param;
		PaAsioStreamInfo inInfo = { sizeof(PaAsioStreamInfo), paASIO, 1, paAsioUseChannelSelectors, _InChannels };
		PaAsioStreamInfo outInfo = { sizeof(PaAsioStreamInfo), paASIO, 1, paAsioUseChannelSelectors, _OutChannels };

		pj_status_t st = pjmedia_aud_dev_default_param(0, &param);		
		PJ_CHECK_STATUS(st, ("ERROR configurando dispositivo de sonido"));

		/**
		 * Rellena los parámetros 'pjmedia', en relacion a la gestion del audio.
		 */
		param.dir = (pjmedia_dir)((_NumInChannels > 0 ? PJMEDIA_DIR_CAPTURE : 0) | (_NumOutChannels > 0 ? PJMEDIA_DIR_PLAYBACK : 0));
		param.clock_rate = SipAgent::SndSamplingRate;
		param.channel_count = CHANNEL_COUNT;
		param.in_channel_count = _NumInChannels;
		param.out_channel_count = _NumOutChannels;
		param.samples_per_frame = ((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000);
		param.in_samples_per_frame = param.samples_per_frame * _NumInChannels;
		param.out_samples_per_frame = param.samples_per_frame * _NumOutChannels;
		param.bits_per_sample = BITS_PER_SAMPLE;
		param.non_interleaved = PJ_TRUE;
		param.inHostApiSpecificStreamInfo = &inInfo;
		param.outHostApiSpecificStreamInfo = &outInfo;

		/**
		 * Crea el STREAM de audio.
		 *	- RecCB: Callback Recepcion STREAM audio
		 *	- PlayCB: Callback Fin Transmision STREAM audio.
		 *
		 * INFO: http://www.pjsip.org/pjmedia/docs/html/group__s2__audio__device__reference.htm#ga476ecc42ccb9744bbc40e42d457ad1aa
		 */
		st = pjmedia_aud_stream_create(&param, &RecCb, &PlayCb, NULL, &_SndDev);
		PJ_CHECK_STATUS(st, ("ERROR creando dispositivo de sonido"));

		try
		{
			/**
			 * Arranca el STREAM de audio.
			 */
			st = pjmedia_aud_stream_start(_SndDev);
			PJ_CHECK_STATUS(st, ("ERROR iniciando dispositivo de sonido"));
		}
		catch (...)
		{
			pjmedia_aud_stream_destroy(_SndDev);
			_SndDev = NULL;
			throw;
		}
	}
	else
	{
		pj_status_t st = pjsua_set_null_snd_dev();
		PJ_CHECK_STATUS(st, ("ERROR creando reloj para el mezclador"));
	}
#else
	pj_status_t st = pjsua_set_null_snd_dev();
	PJ_CHECK_STATUS(st, ("ERROR Hay que utilizar ASIO!!!!"));
#endif


	pj_status_t st = pjsua_start();
	PJ_CHECK_STATUS(st, ("ERROR arrancando agente Sip"));	
}

/**
 * Stop. Rutina de Parada del Módulo
 */
void SipAgent::Stop()
{
	if (_Lock != NULL)
	{
		/**
		 * Cuelga todas las llamadas activas en el módulo.
		 */
		Guard lock(_Lock);
		pjsua_call_hangup_all();

		/**
		 * Espera a que todas las llamadas se hayan cerrado.
		 */
CHECK_CALLS:	
		for (int i = 0; i < PJ_ARRAY_SIZE(pjsua_var.calls); ++i) 
		{
			if (pjsua_var.calls[i].inv != NULL) 
			{
				pj_thread_sleep(100);
				goto CHECK_CALLS;
			}
		}

		/**
		 * Cierra el Socket Remoto ???
		 */
		if (_RemoteSock)
		{
			pj_activesock_close(_RemoteSock);
			_RemoteSock = NULL;
		}

		/**
		 * Libera la lista de Dispositivos de Audio.
		 */
		if (_SndDev)
		{
			pjmedia_aud_stream_stop(_SndDev);
			pjmedia_aud_stream_destroy(_SndDev);

			_SndDev = NULL;
		}

		/**
		 * Libera puerto de grabacion
		 */
		if (_RecordPortTel)
		{
			_RecordPortTel->RecSession(false, true);
			delete _RecordPortTel;
			_RecordPortTel = NULL;
		}

		if (_RecordPortRad)
		{
			_RecordPortRad->RecSession(false, true);
			delete _RecordPortRad;
			_RecordPortRad = NULL;
		}

		/**
		 * Libera los 'puertos'.
		 */
		for (unsigned i = 0; i < PJ_ARRAY_SIZE(_SndPorts); i++)
		{
			if (_SndPorts[i])
			{
				delete _SndPorts[i];
				_SndPorts[i] = NULL;
			}
		}

		/**
		 * Libera los 'reproductors wav'.
		 */
		for (unsigned i = 0; i < PJ_ARRAY_SIZE(_WavPlayers); i++)
		{
			if (_WavPlayers[i])
			{
				delete _WavPlayers[i];
				_WavPlayers[i] = NULL;
			}
		}

		/**
		 * Libera los 'grabadores wav'.
		 */
		for (unsigned i = 0; i < PJ_ARRAY_SIZE(_WavRecorders); i++)
		{
			if (_WavRecorders[i])
			{
				delete _WavRecorders[i];
				_WavRecorders[i] = NULL;
			}
		}

		/**
		 * Libera los puertos de Recepcion Radio.
		 */
		for (unsigned i = 0; i < PJ_ARRAY_SIZE(_RdRxPorts); i++)
		{
			if (_RdRxPorts[i])
			{
				delete _RdRxPorts[i];
				_RdRxPorts[i] = NULL;
			}
		}

		/**
		 * Libera los puertos de Recepcion Audio.
		 */
		for (unsigned i = 0; i < PJ_ARRAY_SIZE(_SndRxPorts); i++)
		{
			if (_SndRxPorts[i])
			{
				delete _SndRxPorts[i];
				_SndRxPorts[i] = NULL;
			}
		}

		/**
		 * Libera la memoria asociada al modulo.
		 */
		_SndRxIds.clear();
		memset(_InChannels, 0, sizeof(_InChannels));
		memset(_OutChannels, 0, sizeof(_OutChannels));


		//WG67Subscription::End();

		lock.Unlock();
		pj_lock_destroy(_Lock);

		pjsua_destroy();
	}
}

/**
 * KeepAliveParams.			Obtiene en ASCII los parámetros del KeepAlive.
 * @param	kaPeriod		Periodo en formato ASCII
 * @param	kaMultiplier	Multiplicador en formato ASCII
 * @return	Nada
 */
void SipAgent::KeepAliveParams(char * kaPeriod, char * kaMultiplier)
{
	Guard lock(_Lock);

	pj_utoa(_KeepAlivePeriod, kaPeriod);
	pj_utoa(_KeepAliveMultiplier, kaMultiplier);
}

/**
 * SetLogLevel. Configura el Nivel de LOG de la clase.
 * @param	level	Nivel de Log
 * @return	Nada
 */
void SipAgent::SetLogLevel(unsigned level)
{
	pjsua_logging_config logCfg;
	pjsua_logging_config_default(&logCfg);

	logCfg.msg_logging = level >= 4;
	logCfg.level = level;
	logCfg.console_level = level;

	if (Cb.LogCb)
	{
		logCfg.decor = 0;
		logCfg.cb = Cb.LogCb;
	}
	else
	{
		logCfg.log_filename = pj_str(const_cast<char*>("./logs/coresip.txt"));
	}

	pjsua_reconfigure_logging(&logCfg);
}

/**
 * SetParams. Configura la lista de parámetros del módulo.
 * @param	info	Puntero a los Parámetros
 *						Flag de Habilitacion de la Escucha
 *						Parámetros asociados al Keepalive
 * @return	Nada
 */
void SipAgent::SetParams(const CORESIP_Params * info)
{
	/**
	 * Habilita la monitorizacion.
	 */
	EnableMonitoring = info->EnableMonitoring != 0;

	Guard lock(_Lock);
	/**
	 * Reconfigura los parámetros de KeepAlive.
	 */
	_KeepAlivePeriod = info->KeepAlivePeriod;
	_KeepAliveMultiplier = info->KeepAliveMultiplier;
}

/**
 * CreateAccount:		Crea una cuenta SIP. No registra en SIP Proxy
 *	- INFO: http://www.pjsip.org/docs/latest/pjsip/docs/html/group__PJSUA__LIB__ACC.htm#gad6e01e4e7ac7c8b1d9f629a6189ca2b2
 * @param	acc			Identificador de una cuenta.
 * @param	defaultAcc	Si es diferente a '0', indica que se creará la cuentra por Defecto.
 * @return	Indentificador de la cuenta.
 */
int SipAgent::CreateAccount(const char * acc, int defaultAcc)
{
	pjsua_acc_config accCfg;
	pjsua_acc_config_default(&accCfg);

	accCfg.id = pj_str(const_cast<char*>(acc));

	pjsua_acc_id accId = PJSUA_INVALID_ID;
	pj_status_t st = pjsua_acc_add(&accCfg, defaultAcc, &accId);
	PJ_CHECK_STATUS(st, ("ERROR creando usuario Sip", "(%s)", acc));

	return accId;
}

/**
 * RegisterInProxy:		Crea y registra una cuenta en SIP Proxy.
 * @param	acc			Num Abonado.
 * @param	defaultAcc	Si es diferente a '0', indica que se creará la cuenta por Defecto.
 * @param	proxy_ip	IP del proxy.
 * @param	expire_seg  Tiempo en el que expira el registro en segundos.
 * @param	username	Si no es necesario autenticación, este parametro será NULL
 * @param   pass		Password. Si no es necesario autenticación, este parametro será NULL
 * @return	Indentificador de la cuenta.
 */
int SipAgent::CreateAccountAndRegisterInProxy(const char * acc, int defaultAcc, const char *proxy_ip, unsigned int expire_seg, const char *username, const char *pass)
{
	pjsua_acc_id accId = PJSUA_INVALID_ID;
	pj_status_t st = PJ_SUCCESS;
	pjsua_acc_config accCfg;
	pjsua_acc_config_default(&accCfg);
	pj_str_t sturi = pj_str(const_cast<char*>("sip:"));
	pj_str_t starr = pj_str(const_cast<char*>("@"));
	pj_str_t stacc = pj_str(const_cast<char*>(acc));	
	pj_str_t stproxip = pj_str(const_cast<char*>(proxy_ip));
	pj_str_t stpp = pj_str(const_cast<char*>(":"));
	pj_str_t st_uaIpAdd = pj_str(const_cast<char*>(uaIpAdd));
	
	char uaport[16];
	pj_utoa(SipAgent::uaPort, uaport);	 
	pj_str_t stuaport = pj_str(const_cast<char*>(uaport));
			
	//"sip:accID@user_agent_ip:5060"
	char cid[128];

	for (int i = 0; i < stacc.slen; i++)
		stacc.ptr[i] = pj_tolower(stacc.ptr[i]);	

	if ((pj_strlen(&sturi) + pj_strlen(&stacc) + 
		pj_strlen(&starr) + pj_strlen(&st_uaIpAdd) + pj_strlen(&stpp) + pj_strlen(&stuaport) + 1) > sizeof(cid))
	{
		st = PJ_ENOMEM;
		PJ_CHECK_STATUS(st, ("ERROR creando usuario Sip. Memoria insuficiente", "(%s)", acc));
		return accId;
	}

	pj_bzero(cid, sizeof(cid));
	accCfg.id = pj_str(cid);
	pj_strcpy(&accCfg.id, &sturi);
	pj_strcat(&accCfg.id, &stacc);
	pj_strcat(&accCfg.id, &starr);
	pj_strcat(&accCfg.id, &pj_str(const_cast<char*>(uaIpAdd)));
	pj_strcat(&accCfg.id, &stpp);
	pj_strcat(&accCfg.id, &stuaport);

	//"sip:proxy_ip"
	char cprox[128];

	if ((pj_strlen(&sturi) + pj_strlen(&stproxip) + 1) > sizeof(cid))
	{
		st = PJ_ENOMEM;
		PJ_CHECK_STATUS(st, ("ERROR creando usuario Sip. Memoria insuficiente", "(%s)", acc));
		return accId;
	}

	pj_bzero(cprox, sizeof(cprox));
	accCfg.reg_uri = pj_str(cprox);
	pj_strcpy(&accCfg.reg_uri, &sturi);
	pj_strcat(&accCfg.reg_uri, &stproxip);

	accCfg.proxy_cnt = 0;
	accCfg.reg_timeout = expire_seg;	
	accCfg.reg_retry_interval = 20;
	if ((username != NULL) && (pass != NULL))
	{
		accCfg.cred_count = 1;
		accCfg.cred_info[0].realm = pj_str(const_cast<char*>("*")); 
		accCfg.cred_info[0].scheme = pj_str(const_cast<char*>("digest")); 
		accCfg.cred_info[0].username = pj_str(const_cast<char*>(username));
		for (int i = 0; i < accCfg.cred_info[0].username.slen; i++)
			accCfg.cred_info[0].username.ptr[i] = pj_tolower(accCfg.cred_info[0].username.ptr[i]);
		accCfg.cred_info[0].data_type = 0;   //Plain text
		accCfg.cred_info[0].data = pj_str(const_cast<char*>(pass));
		for (int i = 0; i < accCfg.cred_info[0].data.slen; i++)
			accCfg.cred_info[0].data.ptr[i] = pj_tolower(accCfg.cred_info[0].data.ptr[i]);
	}

	/*
	char curi_contact[128];
	pj_bzero(curi_contact, sizeof(curi_contact));
	accCfg.force_contact = pj_str(curi_contact);
	pj_strcpy(&accCfg.force_contact, &sturi);
	pj_strcat(&accCfg.force_contact, &stacc);
	pj_strcat(&accCfg.force_contact, &starr);
	pj_strcat(&accCfg.force_contact, &pj_str(const_cast<char*>(uaIpAdd)));

	char uaport[16];
	pj_utoa(SipAgent::uaPort, uaport);	 
	pj_str_t stuaport = pj_str(const_cast<char*>(uaport));
	pj_strcat(&accCfg.force_contact, &stpp);
	pj_strcat(&accCfg.force_contact, &stuaport);
	*/
	
	st = pjsua_acc_add(&accCfg, defaultAcc, &accId);
	PJ_CHECK_STATUS(st, ("ERROR creando usuario Sip", "(%s)", acc));

	pjsua_acc_info info;
	if (st == PJ_SUCCESS)
	{		
		int tries = 10;
		do {
			pj_thread_sleep(5);
			st = pjsua_acc_get_info(accId, &info);
			PJ_CHECK_STATUS(st, ("ERROR en pjsua_acc_get_info", "(%s)", acc));
		} while ((info.status != PJSIP_SC_OK) && (tries-- > 0));
	}	

	return accId;
}

/**
 * DestroyAccount: Borra la Cuenta SIP.
 * @param	id		Identificador de la cuenta. Corresponde al retorno de un 'CreateAccount'
 * @return	Nada
 */
void SipAgent::DestroyAccount(int id)
{
	pj_status_t st = PJ_SUCCESS;
	pjsua_acc_info info;


	//Envia mensaje REGISTER para eliminarse del servidor
	st = pjsua_acc_get_info(id, &info);
	if (info.has_registration)
	{
		//Esta cuenta esta registrada en servidor y se procede a eliminarla
		st = pjsua_acc_set_registration(id, PJ_FALSE);
		PJ_CHECK_STATUS(st, ("ERROR eliminando PJSUA account", "(%d)", id));

		if (st == PJ_SUCCESS)
		{			
			//Espera a que el servidor responda un 200
			int tries = 20;
			do {
				pj_thread_sleep(5);
				st = pjsua_acc_get_info(id, &info);
				PJ_CHECK_STATUS(st, ("ERROR pjsua_acc_get_info", "(%d)", id));
			} while ((info.status != PJSIP_SC_OK) && (--tries > 0));			
		}

		//Necesario para que la funcion pjsua_acc_del no produzca una excepcion al intentar volver a
		//eliminar el registro en el servidor. Es un bug de la libreria
		if (pjsua_var.acc[id].regc) {
			pjsip_regc_destroy(pjsua_var.acc[id].regc);
		}
		pjsua_var.acc[id].regc = NULL;
	} 	

	st = pjsua_acc_del(id);
	PJ_CHECK_STATUS(st, ("ERROR eliminando PJSUA account", "(%d)", id));
	
	return;
}

/**
 * AddSndDevice: Añade un dispositivo de sonido en la Tabla de dispositivos del modulo.
 * @param	info	Puntero a la Informacion del dispositivo de Sonido.
 * @return	Identificador del Dispositivo. Indice del mismo en la tabla de dispositivos.
 */
int SipAgent::AddSndDevice(const CORESIP_SndDeviceInfo * info)
{
	int dev = 0;

	for (; dev < PJ_ARRAY_SIZE(_SndPorts); dev++)
	{
		if (_SndPorts[dev] == NULL)
		{
			break;
		}
	}

	pj_assert(dev < PJ_ARRAY_SIZE(_SndPorts));

	_SndPorts[dev] = new SoundPort(info->Type, info->OsInDeviceIndex, info->OsOutDeviceIndex);

	if (_SndPorts[dev]->InDevIndex >= 0)
	{
		_InChannels[_NumInChannels++] = _SndPorts[dev]->InDevIndex;
	}
	if (_SndPorts[dev]->OutDevIndex >= 0)
	{
		_OutChannels[_NumOutChannels++] = _SndPorts[dev]->OutDevIndex;
	}

	return dev;
}

/**
 * CreateWavPlayer: Crea un 'Reproductor Wav'
 * @param	file	Path al fichero Wav
 * @param	loop	Se reproduce indefinidamente Si/No
 * @return	Identificador del Reproductor. Corresponde al Indice de la tabla de control.
 */
int SipAgent::CreateWavPlayer(const char * file, bool loop)
{
	int id = 0;

	/**
	 * Busca un lugar libre en la tabla de 'retproductores wav'
	 */
	for (; id < PJ_ARRAY_SIZE(_WavPlayers); id++)
	{
		if (_WavPlayers[id] == NULL)
		{
			break;
		}
	}

	pj_assert(id < PJ_ARRAY_SIZE(_WavPlayers));
	/**
	 * Crea el Reproductor a través de la clase @ref WavPlayer.
	 */
	_WavPlayers[id] = new WavPlayer(file, PTIME, loop, OnWavPlayerEof, (void*)(size_t)id);
	return id;
}

/**
 * DestroyWavPlayer: Destruye un 'Reproductor Wav'
 * @param	id		 Identificador del 'Reproductor Wav'
 * @return	Nada
 */
void SipAgent::DestroyWavPlayer(int id)
{
	Guard lock(_Lock);	// Por el autodelete que tienen por puertos wav

	if (_WavPlayers[id] != NULL)
	{
		delete _WavPlayers[id];
		_WavPlayers[id] = NULL;
	}
}

/**
 * CreateWavRecorder: Crea un Grabador Wav
 * @param	file	Puntero al Path del fichero donde grabar.
 * @return	identificador del grabador.
 */
int SipAgent::CreateWavRecorder(const char * file)
{
	int id = 0;

	/**
	 * Busca un lugar libe en la tabla de control.
	 */
	for (; id < PJ_ARRAY_SIZE(_WavRecorders); id++)
	{
		if (_WavRecorders[id] == NULL)
		{
			break;
		}
	}

	pj_assert(id < PJ_ARRAY_SIZE(_WavRecorders));
	/**
	 * Crea el 'grabador' a través de la clase @ref WavRecorder
	 */
	_WavRecorders[id] = new WavRecorder(file);
	return id;
}

/**
 * DestroyWavRecorder: Destruye un Grabador Wav.
 * @param	id		 Identificador del Grabador
 * @return	Nada
 */
void SipAgent::DestroyWavRecorder(int id)
{
	if (_WavRecorders[id] != NULL)
	{
		delete _WavRecorders[id];
		_WavRecorders[id] = NULL;
	}
}

/**
 * CreateRdRxPort: Crea un 'PORT' de Recepcion Radio.
 * @param	info	Puntero a la Informacion del PORT.
 * @param	localIp	Puentero a la direccion Ip de la Máquina.
 * @return	identificador del 'PORT'. Indice de la tabla de control.
 */
int SipAgent::CreateRdRxPort(const CORESIP_RdRxPortInfo * info, const char * localIp)
{
	int id = 0;

	for (; id < PJ_ARRAY_SIZE(_RdRxPorts); id++)
	{
		if (_RdRxPorts[id] == NULL)
		{
			break;
		}
	}

	pj_assert(id < PJ_ARRAY_SIZE(_RdRxPorts));

	_RdRxPorts[id] = new RdRxPort(SAMPLING_RATE, CHANNEL_COUNT, BITS_PER_SAMPLE, PTIME,
		info->ClkRate, info->ChannelCount, info->BitsPerSample, info->FrameTime, localIp, info->Ip, info->Port);
	return id;
}

/**
 * DestroyRdRxPort: Destruye un PORT de Recepción Radio.
 * @param	id		Identificador del PORT
 * @return	Nada
 */
void SipAgent::DestroyRdRxPort(int id)
{
	if (_RdRxPorts[id] != NULL)
	{
		delete _RdRxPorts[id];
		_RdRxPorts[id] = NULL;
	}
}

/**
 * CreateSndRxPort: Crea un PORT de Recepcion de Audio.
 * @param	name	Puntero al Nombre del Puerto
 * @return	Identificador del PORT.
 */
int SipAgent::CreateSndRxPort(const char * name)
{
	Guard lock(_Lock);
	int id = 0;

	for (; id < PJ_ARRAY_SIZE(_SndRxPorts); id++)
	{
		if (_SndRxPorts[id] == NULL)
		{
			break;
		}
	}

	pj_assert(id < PJ_ARRAY_SIZE(_SndRxPorts));

	_SndRxPorts[id] = new SoundRxPort(name, SAMPLING_RATE, CHANNEL_COUNT, BITS_PER_SAMPLE, PTIME);
	_SndRxIds[name] = _SndRxPorts[id];
	return id;
}

/**
 * DestroySndRxPort: Destruye un PORT 'SndRx'
 * @param	id		Identificador del PORT.
 * @return	Nada
 */
void SipAgent::DestroySndRxPort(int id)
{
	Guard lock(_Lock);

	if (_SndRxPorts[id] != NULL)
	{
		_SndRxIds.erase(_SndRxPorts[id]->Id);
		delete _SndRxPorts[id];
		_SndRxPorts[id] = NULL;
	}
}

/**
 * RecConnectSndPort.	...
 * Conecta/desconecta un puerto de sonido al de grabacion
 * @param	on						true - record / false - pause
 * @param  dev			Indice del array _SndPorts. Es dispositivo (microfono) fuente del audio.
 * @return	0 OK, -1  error.
 */
int SipAgent::RecConnectSndPort(bool on, int dev, RecordPort *recordport)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;

	if (dev >= CORESIP_MAX_SOUND_DEVICES)
		return ret;

	if (on)
	{
		if (_SndPorts[dev] != NULL)
		{
			if (_SndPorts[dev]->Slot != PJSUA_INVALID_ID)
			{
				if (!IsSlotValid(_SndPorts[dev]->Slot))
				{
				}
				else if (!recordport->IsSlotConnectedToRecord(_SndPorts[dev]->Slot))
				{
					st = pjsua_conf_connect(_SndPorts[dev]->Slot, recordport->Slot);			
					PJ_CHECK_STATUS(st, ("ERROR conectando puertos", "(%d --> puerto grabacion %d)", _SndPorts[dev]->Slot, recordport->Slot));
				}
			}
		}
	}
	else
	{
		if (_SndPorts[dev] != NULL)
		{
			if (_SndPorts[dev]->Slot != PJSUA_INVALID_ID)
			{
				if (!IsSlotValid(_SndPorts[dev]->Slot))
				{
				}
				else if (recordport->IsSlotConnectedToRecord(_SndPorts[dev]->Slot))
				{
					st = pjsua_conf_disconnect(_SndPorts[dev]->Slot, recordport->Slot);			
					PJ_CHECK_STATUS(st, ("ERROR desconectando puertos", "(%d --> puerto grabacion %d)", _SndPorts[dev]->Slot, recordport->Slot));
				}
			}
		}
	}
	
	return ret;
}

/**
 * RecConnectSndPorts.	...
 * Conecta/desconecta los puertos de sonido al de grabacion
 * @param	on						true - record / false - pause
 * @return	0 OK, -1  error.
 */
int SipAgent::RecConnectSndPorts(bool on, RecordPort *recordport)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;

	if (on)
	{
		//Envia el puerto de grabación el Rx de los _SndPorts
		//Conecta los Rx de los dispositivos de sonido (microfonos) al puerto de grabación VoIP
		for (int dev = 0; dev < PJ_ARRAY_SIZE(_SndPorts); dev++)
		{
			if (_SndPorts[dev] != NULL)
			{
				if (_SndPorts[dev]->Slot != PJSUA_INVALID_ID)
				{
					if (!IsSlotValid(_SndPorts[dev]->Slot))
					{
						_SndPorts[dev]->Slot = PJSUA_INVALID_ID;
					}
					else if (!recordport->IsSlotConnectedToRecord(_SndPorts[dev]->Slot))
					{
						st = pjsua_conf_connect(_SndPorts[dev]->Slot, recordport->Slot);			
						PJ_CHECK_STATUS(st, ("ERROR conectando puertos", "(%d --> puerto grabacion %d)", _SndPorts[dev]->Slot, recordport->Slot));
					}
				}
			}
		}				
	}
	else
	{
		//Corta el envío al puerto de grabación del Rx de los _SndPorts
		//Desconecta los Rx de los dispositivos de sonido (microfonos) del puerto de grabación VoIP
		for (int dev = 0; dev < PJ_ARRAY_SIZE(_SndPorts); dev++)
		{			
			if (_SndPorts[dev] != NULL)
			{
				if (_SndPorts[dev]->Slot != PJSUA_INVALID_ID)
				{
					if (!IsSlotValid(_SndPorts[dev]->Slot))
					{
						_SndPorts[dev]->Slot = PJSUA_INVALID_ID;
					}
					else if (recordport->IsSlotConnectedToRecord(_SndPorts[dev]->Slot))
					{
						st = pjsua_conf_disconnect(_SndPorts[dev]->Slot, recordport->Slot);			
						PJ_CHECK_STATUS(st, ("ERROR desconectando puertos", "(%d --> puerto grabacion %d)", _SndPorts[dev]->Slot, recordport->Slot));
					}
				}
			}
		}
	}
	
	return ret;
}

/**
 * NumConfirmedCalls.	...
 * Retorna Retorna el número de llamadas confirmadas (en curso)
 * @return	Numero de llamadas confirmadas.
 */
unsigned SipAgent::NumConfirmedCalls()
{
	unsigned ret = 0;
	unsigned call_count = 512;
	pjsua_call_id ids[512];
	pj_status_t st = pjsua_enum_calls(ids, &call_count);
	PJ_CHECK_STATUS(st, ("ERROR enum calls in  SipAgent::RecordTel"));

	for (unsigned i = 0; i < call_count; i++)
	{
		pjsua_call_info info;
		st = pjsua_call_get_info(ids[i], &info);

		if (st == PJ_SUCCESS)
		{
			if ((info.state == PJSIP_INV_STATE_CONNECTING || info.state == PJSIP_INV_STATE_CONFIRMED) &&
				info.media_status == PJSUA_CALL_MEDIA_ACTIVE)
				ret++;
		}
	}
	return ret;
}

/**
 * IsSlotValid.	...
 * Retorna si el Slot del puerto es válido. Puede ser qye ya no lo sea porque
 * la llamada ha finalizado
 * @param	slot. slot a validar
 * @return	true o false.
 */
bool SipAgent::IsSlotValid(pjsua_conf_port_id slot)
{
	pjsua_conf_port_info info;

	if (slot == PJSUA_INVALID_ID)
	{
		return false;
	}

	pj_status_t st = pjsua_conf_get_port_info(slot, &info);
	if (st == PJ_SUCCESS) 
	{
		return true;
	}
	else
	{
		return false;
	}
}

/**
 * RecCallStart.	...
 * Envía Inicio llamada telefonia al grabador
 * @param	dir			Direccion de la llamada 
 * @param	priority	Prioridad
 * @param	ori_uri		URI del llamante
 * @param	dest_uri	URI del llamado
 * @return	0 OK, -1  error.
 */
int SipAgent::RecCallStart(int dir, CORESIP_Priority priority, const pj_str_t *ori_uri, const pj_str_t *dest_uri)
{
	if (!_RecordPortTel) return -1;
	return _RecordPortTel->RecCallStart(dir, priority, ori_uri, dest_uri);
}

/**
 * RecCallEnd.	...
 * Envía Fin llamada telefonia al grabador
 * @param	cause		causa de desconexion
 * @param	disc_origin		origen de la desconexion
 * @return	0 OK, -1  error.
 */
int SipAgent::RecCallEnd(int cause, int disc_origin)
{
	if (!_RecordPortTel) return -1;
	return _RecordPortTel->RecCallEnd(cause, disc_origin);
}

/**
 * RecCallConnected.	...
 * Envía llamada telefonia establecida al grabador
 * @param	cause		causa de desconexion
 * @param	disc_origin		origen de la desconexion
 * @return	0 OK, -1  error.
 */
int SipAgent::RecCallConnected(const pj_str_t *connected_uri)
{
	if (!_RecordPortTel) return -1;
	return _RecordPortTel->RecCallConnected(connected_uri);
}

/**
 * RecHold.	...
 * Envia evento Hold.
 * @param	on	true=ON, false=OFF
 * @param	llamante true=llamante false=llamado
 * @return	0 OK, -1  error.
 */
int SipAgent::RecHold(bool on, bool llamante)
{
	if (!_RecordPortTel) return -1;
	return _RecordPortTel->RecHold(on, llamante);
}

/**
 *	RdPttEvent. Se llama cuando hay un evento de PTT
 *  @param  on			true=ON/false=OFF
 *	@param	freqId		Identificador de la frecuencia
 *  @param  dev			Indice del array _SndPorts. Es dispositivo (microfono) fuente del audio.
 */
void SipAgent::RdPttEvent(bool on, const char *freqId, int dev)
{
	if (!_RecordPortRad) return;
	_RecordPortRad->RecPTT(on, freqId, dev);
}

/**
 *	RdSquEvent. Se llama cuando hay un evento de SQUELCH
 *  @param  on			true=ON/false=OFF
 *  @param  dev			Indice del array _SndPorts. Es dispositivo (microfono) fuente del audio.
 *	@param	freqId		Identificador de la frecuencia 
 */
void SipAgent::RdSquEvent(bool on, const char *freqId, int dev)
{
	if (!_RecordPortRad) return;
	_RecordPortRad->RecSQU(on, freqId, dev);
}

/**
 * BridgeLink: Rutina para Conectar o Desconectar 'puertos'
 * @param	srcType		Tipo de Puerto Origen. Pueden ser del siguiente tipo:
 *							- CORESIP_CALL_ID: Un flujo RTP
 *							- CORESIP_SNDDEV_ID: Un dispositivo de Audio.
 *							- CORESIP_WAVPLAYER_ID: Reproductor de Fichero Wav.
 *							- CORESIP_RDRXPORT_ID: 
 *							- CORESIP_SNDRXPORT_ID:
 * @param	src			Identificador del Puerto.
 * @param	dstType		Tipo de Puerto de Destino. Pueden ser del siguiente tipo
 *							- CORESIP_CALL_ID: Un flujo RTP
 *							- CORESIP_SNDDEV_ID: Un dispositivo de Audio.
 *							- CORESIP_WAVRECORDER_ID: Grabador de Fichero Wav.
 * @param	dst			Identificador del Puerto de Destino,
 * @param	on			Indica Conexión o Desconexión.
 * @return	Nada
 */
void SipAgent::BridgeLink(int srcType, int src, int dstType, int dst, bool on)
{
	pjsua_conf_port_id conf_src, conf_dst;

	Guard lock(_Lock);

	/**
	 * Obtiene el Puerto de Conferencia asociado al origen.
	 */
	switch (srcType)
	{
	case CORESIP_CALL_ID:		
		conf_src = pjsua_call_get_conf_port(src);
		break;
	case CORESIP_SNDDEV_ID:
		conf_src = _SndPorts[src]->Slot;
		break;
	case CORESIP_WAVPLAYER_ID:
		conf_src = _WavPlayers[src]->Slot;
		break;
	case CORESIP_RDRXPORT_ID:
		conf_src = _RdRxPorts[src]->Slot;
		break;
	case CORESIP_SNDRXPORT_ID:
		conf_src = _SndRxPorts[src & 0x0000FFFF]->Slots[src >> 16];
		break;
	default:
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Tipo origen invalido");
	}

	/**
	 * Obtiene el Puerto de Conferencia asociado al Destino.
	 */
	switch (dstType)
	{
	case CORESIP_CALL_ID:
		conf_dst = pjsua_call_get_conf_port(dst);

		if (srcType == CORESIP_SNDDEV_ID && _RecordPortTel != NULL)
		{
			//Si se conecta un puerto de sonido hacia un puerto del tipo telefonia
			//Entonces hay que conectar ese puerto de sonido con la grabacion
			if (on)
			{
				RecConnectSndPort(on, src, _RecordPortTel);
			}
			else
			{
				int ncalls = NumConfirmedCalls();
				if (ncalls <= 1)
				{
					//Si hay más de una llamada en curso entonces no cortamos el puerto de sonido de la grabación.
					RecConnectSndPort(on, src, _RecordPortTel);
				}
			}			
		}

		break;
	case CORESIP_SNDDEV_ID:
		conf_dst = _SndPorts[dst]->Slot;

		if (srcType == CORESIP_CALL_ID)
		{
			//Si el origen es una llamada telefónica lo añadimos o quitamos del array SlotsToSndPorts
			//del puerto de grabacion de telefonia
			if (on && _RecordPortTel != NULL) 
			{
				_RecordPortTel->Add_SlotsToSndPorts(conf_src);
			}
			else if (_RecordPortTel != NULL)
			{
				_RecordPortTel->Del_SlotsToSndPorts(conf_src);
			}
		}

		if (srcType == CORESIP_RDRXPORT_ID)
		{
			//Si el origen es un puerto RDRX lo añadimos o quitamos del array SlotsToSndPorts
			//del puerto de grabacion de radio
			if (on && _RecordPortRad != NULL) 
			{
				_RecordPortRad->Add_SlotsToSndPorts(conf_src);
			}
			else if (_RecordPortRad != NULL)
			{
				_RecordPortRad->Del_SlotsToSndPorts(conf_src);
			}
		}

		break;
	case CORESIP_WAVRECORDER_ID:		
		conf_dst = _WavRecorders[dst]->Slot;
		break;
	default:
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Tipo destino invalido");
	}

	/**
	 * Conecta o Desconecta los puertos.
	 */
	if ((conf_src != PJSUA_INVALID_ID) && (conf_dst != PJSUA_INVALID_ID))
	{
		if (on)
		{
			pj_status_t st = pjsua_conf_connect(conf_src, conf_dst);
			PJ_CHECK_STATUS(st, ("ERROR conectando puertos", "(%d --> %d)", conf_src, conf_dst));
		}
		else
		{
			pj_status_t st = pjsua_conf_disconnect(conf_src, conf_dst);
			PJ_CHECK_STATUS(st, ("ERROR desconectando puertos", "(%d --> %d)", conf_src, conf_dst));
		}
	}
}

/**
 * SendToRemote:	Configura El puerto de Sonido apuntado para los envios UNICAST de Audio.
 * @param	dev		Identificador de Dispositivo 'SndPort'
 * @param	on		Indica si se configura o se desconfigura.
 * @param	id		Literal que identifica al emisor.
 * @param	ip		Direccion IP de Destino.
 * @param	port	Puerto UDP de Destino.
 * @return	nada
 */
void SipAgent::SendToRemote(int typeDev, int dev, bool on, const char * id, const char * ip, unsigned port)
{	
	if (typeDev == CORESIP_SNDDEV_ID)
	{
		_SndPorts[dev]->Remote(on, id, ip, port);
		if (_RecordPortRad != NULL)
		{
			//Se conecta el puerto de audio TX (microfono) pasado por dev al puerto de grabacion
			SipAgent::RecConnectSndPort(on, dev, _RecordPortRad);
		}
	}
}

/**
 * ReceiveFromRemote:	Configura el múdulo para recibir audio por Multicast.
 * @param	localIp		Dirección Ip Local
 * @param	mcastIp		Direccion Ip Multicast de escucha.
 * @param	mcastPort	Puerto UDP de escucha
 * @return	Nada
 */
void SipAgent::ReceiveFromRemote(const char * localIp, const char * mcastIp, unsigned mcastPort)
{
	pj_sockaddr_in addr, mcastAddr;
	pj_sockaddr_in_init(&addr, &(pj_str(const_cast<char*>(localIp))), (pj_uint16_t)mcastPort);
	pj_sockaddr_in_init(&mcastAddr, &(pj_str(const_cast<char*>(mcastIp))), (pj_uint16_t)mcastPort);

	/**
	 * Crea el socket de recepcion.
	 */
	pj_status_t st = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &_Sock);
	PJ_CHECK_STATUS(st, ("ERROR creando socket para puerto de recepcion sndDev radio"));

	try
	{
		int on = 1;
		/**
		 * Configura el socket para que sea 'reutizable' y habilita (joint) el grupo Multicast.
		 */
		pj_sock_setsockopt(_Sock, pj_SOL_SOCKET(), SO_REUSEADDR, (void *)&on, sizeof(on));

		st = pj_sock_bind(_Sock, &addr, sizeof(addr));
		PJ_CHECK_STATUS(st, ("ERROR enlazando socket para puerto de recepcion sndDev radio", "[Ip=%s][Port=%d]", localIp, mcastPort));

		struct ip_mreq	mreq;
		mreq.imr_multiaddr.S_un.S_addr = mcastAddr.sin_addr.s_addr;
		mreq.imr_interface.S_un.S_addr = addr.sin_addr.s_addr;

		st = pj_sock_setsockopt(_Sock, IPPROTO_IP, pj_IP_ADD_MEMBERSHIP(), (void *)&mreq, sizeof(mreq));
		PJ_CHECK_STATUS(st, ("ERROR añadiendo socket a multicast para puerto de recepcion sndDev radio", "[Mcast=%s][Port=%d]", mcastIp, mcastPort));

		/**
		 * Configura el Callback para Recibir Datos.
		 */ 
		pj_activesock_cb cb = { NULL };
		cb.on_data_recvfrom = &OnDataReceived;

		/**
		 * Configura el socket para que funcione como 'Active Socket':
		 *	- http://www.pjsip.org/pjlib/docs/html/group__PJ__ACTIVESOCK.htm#_details
		 */
		st = pj_activesock_create(pjsua_var.pool, _Sock, pj_SOCK_DGRAM(), NULL, pjsip_endpt_get_ioqueue(pjsua_get_pjsip_endpt()), &cb, NULL, &_RemoteSock);
		PJ_CHECK_STATUS(st, ("ERROR creando servidor de lectura para puerto de recepcion sndDev radio"));

		/**
		 * Activa la Recepcion en el Socket.
		 */ 
		unsigned samplesPerFrame = SAMPLING_RATE * CHANNEL_COUNT * PTIME / 1000;
		st = pj_activesock_start_recvfrom(_RemoteSock, pjsua_var.pool, sizeof(RemotePayload), 0);
		PJ_CHECK_STATUS(st, ("ERROR iniciando lectura en puerto de recepcion sndDev radio"));
	}
	catch (...)
	{
		/**
		 * Tratamiento de Errores. Excepciones.
		 *	- Cierra los socket's abierto
		 *	- Reinicializa las variables globales.
		 */
		if (_RemoteSock)
		{
			pj_activesock_close(_RemoteSock);
			_RemoteSock = NULL;
		}
		else if (_Sock != PJ_INVALID_SOCKET)
		{
			pj_sock_close(_Sock);
		}
		_Sock = PJ_INVALID_SOCKET;

		throw;
	}
}

/**
 * SetVolume: Ajusta el volumen de Recepcion en un PORT.
 * @param	idType		Tipo de PORT.
 *							- CORESIP_CALL_ID: Un flujo RTP
 *							- CORESIP_SNDDEV_ID: Un dispositivo de Audio.
 *							- CORESIP_WAVPLAYER_ID: Reproductor de Fichero Wav.
 *							- CORESIP_RDRXPORT_ID: 
 *							- CORESIP_SNDRXPORT_ID:
 * @param	id			Identificador del PORT.
 * @param	volume		Valor del volumen. Rango 0 (Mute) y 100 (Maximo)
 * @return	Nada
 *
 * INFO: http://www.pjsip.org/pjmedia/docs/html/group__PJMEDIA__CONF.htm#ga8895228fdc9b7d6892320aa03b198574
 */
void SipAgent::SetVolume(int idType, int id, unsigned volume)
{
	pjsua_conf_port_id conf_id;

	/**
	 * Obtiene el Puerto de Conferencia asociado al origen.
	 */
	switch (idType)
	{
	case CORESIP_CALL_ID:
		conf_id = pjsua_call_get_conf_port(id);
		break;
	case CORESIP_SNDDEV_ID:
		conf_id = _SndPorts[id]->Slot;
		break;
	case CORESIP_WAVPLAYER_ID:
		conf_id = _WavPlayers[id]->Slot;
		break;
	case CORESIP_RDRXPORT_ID:
		conf_id = _RdRxPorts[id]->Slot;
		break;
	case CORESIP_SNDRXPORT_ID:
		conf_id = _SndRxPorts[id & 0x0000FFFF]->Slots[id >> 16];
		break;
	default:
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Tipo dispositivo invalido");
	}

	/**
	 * Si no hay error ajusta el volumen.
	 */
	if (conf_id != PJSUA_INVALID_ID)
	{
		pjmedia_conf_adjust_rx_level(pjsua_var.mconf, conf_id, (256 * ((int)volume - 50)) / 100);
	}
}

/**
 * GetVolume: Obtiene el valor de Volumen de recepcion asociado a un PORT.
 * @param	idType		Tipo de PORT.
 *							- CORESIP_CALL_ID: Un flujo RTP
 *							- CORESIP_SNDDEV_ID: Un dispositivo de Audio.
 *							- CORESIP_WAVPLAYER_ID: Reproductor de Fichero Wav.
 *							- CORESIP_RDRXPORT_ID: 
 *							- CORESIP_SNDRXPORT_ID:
 * @param	id			Identificador del PORT.
 * @return	Valor del Volumen. Rango 0 (Mute) y 100 (Maximo)
 */
unsigned SipAgent::GetVolume(int idType, int id)
{
	pjsua_conf_port_id conf_id;

	/**
	 * Obtiene el Puerto de Conferencia asociado al origen.
	 */
	switch (idType)
	{
	case CORESIP_CALL_ID:
		conf_id = pjsua_call_get_conf_port(id);
		break;
	case CORESIP_SNDDEV_ID:
		conf_id = _SndPorts[id]->Slot;
		break;
	case CORESIP_WAVPLAYER_ID:
		conf_id = _WavPlayers[id]->Slot;
		break;
	case CORESIP_RDRXPORT_ID:
		conf_id = _RdRxPorts[id]->Slot;
		break;
	case CORESIP_SNDRXPORT_ID:
		conf_id = _SndRxPorts[id & 0x0000FFFF]->Slots[id >> 16];
		break;
	default:
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Tipo dispositivo invalido");
	}

	/**
	 * Si no hay error, devuelve un valor entre 0 y 100.
	 */
	if (conf_id != PJSUA_INVALID_ID)
	{
		pjmedia_conf_port_info info;
		pjmedia_conf_get_port_info(pjsua_var.mconf, conf_id, &info);

		return (int)((info.rx_adj_level + 128) / 2,56);
	}

	return 0;
}

/**
 * OnRxResponse:	Callback. Se invoca cuando se recibe una 'respuesta' SIP
 *						- Cuando la Respuesta corresponde a un metodo 'option'
 *						- Invoca a una rutina de Aplicacion 'OptionsReceive' Indicando el URI en un string.
 * @param	rdata		Puntero 'pjsip_rx_data' a los datos recibidos.
 * @return	'pj_bool_t' Siempre retorna 'PJ_FALSE', para que continue el tratamiento del evento la propia libreria.
 *
 *	INFO: http://www.pjsip.org/pjsip/docs/html/group__PJSIP__MSG__METHOD.htm#gafdd26e26092275d7f156a0d8efe90b78
 *		  http://www.pjsip.org/pjsip/docs/html/group__PJSIP__MSG__METHOD.htm#ga66b57e1b5645d2ee843141a0e657b0d1
 */
pj_bool_t SipAgent::OnRxResponse(pjsip_rx_data *rdata)
{
	if (pjsip_method_cmp(&rdata->msg_info.cseq->method, pjsip_get_options_method()) == 0)
	{
		char dstUri[CORESIP_MAX_URI_LENGTH + 1] = { 0 };
		pjsip_sip_uri * dst = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.to->uri);

		pj_ansi_snprintf(dstUri, sizeof(dstUri) - 1, "<sip:%.*s@%.*s>", 
			dst->user.slen, dst->user.ptr, dst->host.slen, dst->host.ptr);

		Cb.OptionsReceiveCb(dstUri);
	}

	return PJ_FALSE;
}

/**
 * OnWavPlayerEof: Callback. Se llama al finalizar la reproducción deun fichero wav
 * @param	port		Puntero 'pj_media_port' al puerto Implicado.
 * @param	userData	Datos de Usuario. Por configuracion del sistema debe corresponder al id del Reproductor
 * @return	Si no hay excepciones, retorna siempre PJ_EEOF.
 */
pj_status_t SipAgent::OnWavPlayerEof(pjmedia_port *port, void *userData)
{
	DestroyWavPlayer((int)(size_t)userData);
	return PJ_EEOF;
}

/**
 * OnDataReceived: Callback Recepcion Audio Multicast.
 * @param	asock		Puntero 'pj_activesock' al SOCKET involucrado en el proceso de Recepcion.
 * @param	data		Puntero a los datos recibidos.
 * @param	size		Tamaño de los datos recibidos.
 * @param	src_addr	Puntero a Direccion Ip que envia los datos.
 * @param	addr_len	Longitud del campo de Direccion Ip.
 * @param	status		Estado de los Datos Recibidos.
 * @return	PJ_TRUE
 */
pj_bool_t SipAgent::OnDataReceived(pj_activesock_t * asock, void * data, pj_size_t size, const pj_sockaddr_t *src_addr, int addr_len, pj_status_t status)
{
	/**
	 * Comprueba que no hay error en la recepcion y es una trama del sistema.
	 */
	if ((status == PJ_SUCCESS) && (size == sizeof(RemotePayload)))
	{
		RemotePayload * pl = (RemotePayload*)data;
		pj_assert(pl->Size == (SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8)));
		Guard lock(_Lock);

		/**
		 * Envia la trama a todos los puertos tipo @ref SoundRxPort
		 */
		std::map<std::string, SoundRxPort*>::iterator it = _SndRxIds.find(pl->SrcId);
		if (it != _SndRxIds.end())
		{
			it->second->PutFrame(pl->SrcType, pl->Data, pl->Size);
		}
	}

	return PJ_TRUE;
}

#ifdef PJ_USE_ASIO

/**
 * RecCb: Callback. Se llama cada vez que se captura una trama de Audio en Recepcion (en RTP).
 * - Se compila solo con la Macro 'PJ_USE_ASIO' activada.
 *
 * @param	userData	Puntero a datos de Usuario. 
 * @param	frame		Puntero 'pjmedia_frame' a la trama recibida.
 * @return	PJ_SUCCESS
 *
 * INFO: http://www.pjsip.org/pjmedia/docs/html/group__s2__audio__device__reference.htm#ga476ecc42ccb9744bbc40e42d457ad1aa
 */
pj_status_t SipAgent::RecCb(void * userData, pjmedia_frame * frame)
{
	/**
	 * Comprueba que la trama corresponde por tamaño a una trama RTP del sistema.
	 */
	pj_assert(frame->size == ((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000) * (BITS_PER_SAMPLE / 8) * _NumInChannels);

	/**
	 * Envia la trama recibida a todos los canales de Entrada. Los objetos son del tipo @ref SoundPort
	 */ 
	if (_NumInChannels)
	{
		Guard lock(_Lock);

		for (int i = 0, inIdx = 0; i < PJ_ARRAY_SIZE(_SndPorts); i++)
		{
			SoundPort * sndPort = _SndPorts[i];

			if (sndPort && (sndPort->InDevIndex == _InChannels[inIdx]))
			{
				sndPort->SetInBuf(frame, inIdx * ((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000) * (BITS_PER_SAMPLE / 8));
				inIdx++;
			}
		}

	}

	/**
	 * Hace un 'get_frame' al @ref _ConfMasterPort.
	 * En los SipAgent asociados a las llamadas Radio (que ejecuta nodebox.master), las tramas recibidas se han
	 * transmitido al grupo mcast asociado, y además se han introducido en el Buffer de Jitter, por lo cual hay
	 * que sacarlas y ya no se utilizan para nada.
	 */
	if (!_NumOutChannels)
	{
		pjmedia_frame f = { PJMEDIA_FRAME_TYPE_NONE };
		pj_int16_t buf[SAMPLES_PER_FRAME];

		f.buf = buf;
		f.size = SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8);
		f.timestamp.u64 = frame->timestamp.u64;

		_ConfMasterPort->get_frame(_ConfMasterPort, &f);
	}

			
	/** AGL. Tick Multimedia */
	if (_wp2r != 0)	
	{
		if (_wp2r->Tick()==FALSE)		
		{		
			DestroyWavPlayer2Remote();			
		}		
	}
	/** */

	return PJ_SUCCESS;
}

/**
 * PlayCb: Callback. Se llamaa cada vez que los dispositivos (puertos) necesitan AUDIO que reproducir.
 * - Se compila solo con la Macro 'PJ_USE_ASIO' activada.
 *
 * @param	userData	Puntero a datos recibidos.
 * @param	frame		Puntero 'pjmedia_frame' a ...
 * @return	PJ_SUCCESS
 *
 * INFO: http://www.pjsip.org/pjmedia/docs/html/group__s2__audio__device__reference.htm#ga476ecc42ccb9744bbc40e42d457ad1aa
 */
pj_status_t SipAgent::PlayCb(void * userData, pjmedia_frame * frame)
{
	/**
	 * Comprueba que el frame enviado corresponde a una trama del sisema.
	 */
	pj_assert(frame->size == ((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000) * (BITS_PER_SAMPLE / 8) * _NumOutChannels);

	/**
	 * Solo si hay canales (puertos) de salida.
	 */ 
	if (_NumOutChannels)
	{
		Guard lock(_Lock);

		/**
		 * Envia a cada puerto de salida configurado la trama.
		 */
		for (int i = 0, outIdx = 0; i < PJ_ARRAY_SIZE(_SndPorts); i++)
		{
			SoundPort * sndPort = _SndPorts[i];

			if (sndPort && (sndPort->OutDevIndex == _OutChannels[outIdx]))
			{
				sndPort->SetOutBuf(frame, outIdx * ((SipAgent::SndSamplingRate * CHANNEL_COUNT * PTIME) / 1000) * (BITS_PER_SAMPLE / 8));
				outIdx++;
			}
		}

		/**
		 * Hace un 'get_frame' al @ref _ConfMasterPort. ???
		 */
		pjmedia_frame f = { PJMEDIA_FRAME_TYPE_NONE };
		pj_int16_t buf[SAMPLES_PER_FRAME];

		f.buf = buf;
		f.size = SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8);
		f.timestamp.u64 = frame->timestamp.u64;

		_ConfMasterPort->get_frame(_ConfMasterPort, &f);
	}

	return PJ_SUCCESS;
}

#endif

/** AGL */
WavPlayerToRemote *SipAgent::_wp2r=0;
int SipAgent::CreateWavPlayer2Remote(const char *filename, const char * id, const char * ip, unsigned port)
{
	if (_wp2r == 0)
	{
		_wp2r = new WavPlayerToRemote(filename, PTIME, 0);
		_wp2r->Send2Remote(id, ip, port);
	}
	return 0;
}

void SipAgent::DestroyWavPlayer2Remote()
{
	if (_wp2r != 0)
	{
		delete _wp2r;
		_wp2r=0;
	}
}

/** */
