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
#include "PresenceManag.h"
#include "ExtraParamAccId.h"

#include <iphlpapi.h>


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
	&SipAgent::OnRxRequest,											/* on_rx_request()			*/
	&SipAgent::OnRxResponse,				/* on_rx_response()			*/
	&SipCall::OnTxRequest,											/* on_tx_request.			*/
	&SipCall::OnTxRequest,											/* on_tx_response()			*/
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
 *	SipAgent::FrecDesp: Gestor de grupos de climax.
 */
FrecDesp *	SipAgent::_FrecDesp = NULL;

/**
 *	SipAgent::_PresenceManager: Gestor de presencias
 */
PresenceManag *	SipAgent::_PresenceManager = NULL;

/**
 *	SipAgent::_ConfManager: Gestor de conferencias
 */
SubsManager<ConfSubs> *	SipAgent::_ConfManager = NULL;

/**
 *	SipAgent::_DlgManager: Gestor de subscripciones al evento de dialogo
 */
SubsManager<DlgSubs> *	SipAgent::_DlgManager = NULL;

//Cancela eco entre altavoz y microfono cuando 
//telefonia sale por altavoz
pjmedia_echo_state *SipAgent::_EchoCancellerLCMic = NULL;	
pj_bool_t SipAgent::_AltavozLCActivado = PJ_FALSE;			//Si true, el altavoz LC esta activado reproduciendo audio
pj_lock_t *SipAgent::_ECLCMic_mutex = NULL;					//Mutex para el cancelador de eco

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


bool SipAgent::IsRadio = false;
char SipAgent::StaticContact[256];
pjsip_sip_uri *SipAgent::pContacUrl = NULL;

char SipAgent::uaIpAdd[32];
char SipAgent::HostId[33];
unsigned int SipAgent::uaPort = 0;
pjsua_transport_id SipAgent::SipTransportId = PJSUA_INVALID_ID;

unsigned SipAgent::_TimeToDiscardRdInfo = 0;	//Tiempo durante el cual no se envia RdInfo al Nodebox tras un PTT OFF

pj_bool_t SipAgent::_HaveRdAcc = PJ_FALSE;		//Si vale true, entonces algun account del agente es de lipo radio GRS

/*Parametros para simulador de radio*/
unsigned SipAgent::_Radio_UA = 0;				//Con valor distinto de 0, indica que se comporta como un agente de radio

pjmedia_port *SipAgent::_Tone_Port = NULL;				//Puerto de la pjmedia para reproducir un tono
pjsua_conf_port_id SipAgent::_Tone_Port_Id = PJSUA_INVALID_ID;
stCoresip_Local_Config SipAgent::Coresip_Local_Config = {5};

pj_bool_t SipAgent::PTT_local_activado = PJ_FALSE;		//Estado del PTT local


/**
 * Init. Inicializacion de la clase:
 * - Incializa las Variables de la Clase, tanto locales como de librería.
 * - Crea el Agente SIP.
 * @param	cfg		Puntero a la configuracion local.
 * @return	Nada
 */
void SipAgent::Init(const CORESIP_Config * cfg)
{
	pj_status_t st = PJ_FALSE;

	ReadiniFile();
		
	for (unsigned i = 0; i < PJ_ARRAY_SIZE(_SndPorts); i++)
	{
		_SndPorts[i] = NULL;
	}

	for (unsigned i = 0; i < PJ_ARRAY_SIZE(_WavPlayers); i++)
	{
		_WavPlayers[i] = NULL;
	}

	for (unsigned i = 0; i < PJ_ARRAY_SIZE(_RdRxPorts); i++)
	{
		_RdRxPorts[i] = NULL;
	}

	for (unsigned i = 0; i < PJ_ARRAY_SIZE(_SndRxPorts); i++)
	{
		_SndRxPorts[i] = NULL;
	}

	for (unsigned i = 0; i < PJ_ARRAY_SIZE(_WavRecorders); i++)
	{
		_WavRecorders[i] = NULL;
	}

	_TimeToDiscardRdInfo = cfg->TimeToDiscardRdInfo;
	_Radio_UA = cfg->Radio_UA;

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
	st = pjsua_create();
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

		/** Para Utilizar en las CALLBACKS */
		IsRadio = strcmp(cfg->HostId,"UV5KRadioUA")==0;
		sprintf(StaticContact, "<sip:%s@%s>", cfg->HostId, cfg->IpAddress);
		
		pContacUrl =  pjsip_sip_uri_create(pjsua_var.pool, 0);
		pj_strdup2(pjsua_var.pool, &pContacUrl->user, cfg->HostId);
		pj_strdup2(pjsua_var.pool, &pContacUrl->host, cfg->IpAddress);
		pContacUrl->port = cfg->Port;
		/*****/

		/**
		 * Configuracion del Bloque de Configuracion de 'pjsua'.
		 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__config.htm
		 */		
		if (cfg->max_calls > PJSUA_MAX_CALLS) uaCfg.max_calls = 30;		
		else uaCfg.max_calls = cfg->max_calls;

		uaCfg.user_agent = pj_str("U5K-UA/1.0.0");		

		uaCfg.cb.on_call_state = &SipCall::OnStateChanged;						// Trata los Cambios de estado de una Llamada.
		uaCfg.cb.on_incoming_call = &SipCall::OnIncommingCall;					// Trata la Notificacion de Llamada Entrante.
		uaCfg.cb.on_call_media_state = &SipCall::OnMediaStateChanged;			// Trata la conexion Desconexion de la Media a un dispositivo.
		uaCfg.cb.on_call_transfer_request = &SipCall::OnTransferRequest;		// Trata la notificación de transferencia de llamada.
		uaCfg.cb.on_call_transfer_status = &SipCall::OnTransferStatusChanged;	// Trata los cambios de estado de una llamada previamente transferidad
		uaCfg.cb.on_call_tsx_state = &SipCall::OnTsxStateChanged;				// Trata cambios genericos en el estado de una llamada.
		uaCfg.cb.on_stream_created = &SipCall::OnStreamCreated;					// Gestiona la creación de los stream y su conexion a los puertos de conferencia
		uaCfg.cb.on_stream_destroyed = &SipCall::OnStreamDestroyed;				// Se llama en stop_media_session, justo antes de destruir session. Me servirá
																				// para eliminarlo del grupo climax
		uaCfg.cb.on_pager = &SipAgent::OnPager;									//Notify application on incoming pager (i.e. MESSAGE request).
		uaCfg.nat_type_in_sdp = 0;												// No information will be added in SDP, and parsing is disabled.
		/** WG67_KEY_IN */
		uaCfg.cb.on_incoming_subscribe = NULL;

		/** AGL */
		uaCfg.require_timer=PJ_FALSE;
		uaCfg.require_100rel=PJ_FALSE;


		/**
		 * Configuracion del Bloque de 'LOG' de 'pjsua'.
		 */
		logCfg.msg_logging = cfg->LogLevel >= 4;
		logCfg.level = cfg->LogLevel;
		logCfg.console_level = 3;	// cfg->LogLevel;

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

		st = pjsip_wg67_init_module(pjsua_get_pjsip_endpt());
		PJ_CHECK_STATUS(st, ("ERROR inicialiando modulo WG67KEY-IN"));

		/**
		 * Inicializacion del módulo de conferencia.
		 */
		st = pjsip_conf_init_module(pjsua_get_pjsip_endpt());
		PJ_CHECK_STATUS(st, ("ERROR inicialiando modulo conference"));

		/**
		 * Inicializacion del módulo para eventos de dialogo.
		 */
		st = pjsip_dialog_init_module(pjsua_get_pjsip_endpt());
		PJ_CHECK_STATUS(st, ("ERROR inicialiando modulo dialog"));

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
		transportCfg.public_addr = pj_str(const_cast<char*>(cfg->IpAddress));
		transportCfg.bound_addr = pj_str(const_cast<char*>(cfg->IpAddress));
		strcpy(SipAgent::uaIpAdd, cfg->IpAddress);
		strcpy(SipAgent::HostId, cfg->HostId);
		SipAgent::uaPort = cfg->Port;
		transportCfg.options = PJMEDIA_UDP_NO_SRC_ADDR_CHECKING;

		/**
		 * Crea el Transporte para SIP. UDP en puerto Base.
		 */
		st = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transportCfg, &SipTransportId);
		PJ_CHECK_STATUS(st, ("ERROR creando puertos UDP para Sip", "(%s:%d)", cfg->IpAddress, cfg->Port));	

		pj_sock_t sip_socket = pjsip_udp_transport_get_socket(pjsua_var.tpdata[SipTransportId].data.tp);
		if (sip_socket != PJ_INVALID_SOCKET)
		{
			//Se fuerza que los paquetes salgan por el interfaz que utiliza el agente.
			struct pj_in_addr in_uaIpAdd;
			pj_inet_aton((const pj_str_t *) &pj_str(const_cast<char*>(cfg->IpAddress)), &in_uaIpAdd);
			st = pj_sock_setsockopt(sip_socket, pj_SOL_IP(), PJ_IP_MULTICAST_IF, (void *)&in_uaIpAdd, sizeof(in_uaIpAdd));	
			if (st != PJ_SUCCESS)
				PJ_LOG(3,(__FILE__, "ERROR: setsockopt, PJ_IP_MULTICAST_IF. El transporte SIP no se puede forzar por el interface %s", cfg->IpAddress));
		}

		/**
		 * Crea el Transporte para RTP
		 */

		if (cfg->RtpPorts == 0)
		{
			unsigned int porto = cfg->Port + 2;
			//st = pjsua_media_transports_create(&transportCfg);
			//PJ_CHECK_STATUS(st, ("ERROR creando puertos UDP para RTP", "(%s:%d)", cfg->IpAddress, transportCfg.port));


			//Intentamos varias veces abrir los puertos rtp. 
			//Por si acaso hay otra aplicacion que este abriendo puertos a la vez que esta

			int tries = 5;
			while (tries > 0)
			{
				if (GetRTPPort(&porto) < 0)
				{
					PJ_CHECK_STATUS(PJ_EUNKNOWN, ("ERROR No se pueden obtener los puertos libres necesarios para RTP"));
				}
				transportCfg.port = porto;	
				st = pjsua_media_transports_create(&transportCfg);
				if (st == PJ_SUCCESS)
				{
					break;
				}
				pj_thread_sleep(250);
				tries--;
			}
			if (tries == 0)
			{
				PJ_CHECK_STATUS(PJ_EUNKNOWN, ("ERROR No se pueden obtener los puertos para RTP", " despues de %d intentos", tries));
			}

		}
		else
		{
			transportCfg.port = cfg->RtpPorts;

			st = pjsua_media_transports_create(&transportCfg);
			PJ_CHECK_STATUS(st, ("ERROR creando puertos UDP para RTP", "(%s:%d)", cfg->IpAddress, transportCfg.port));
		}

		PJ_LOG(3,(__FILE__, "Puertos para RTP reservados: (%u - %u), para un maximo de %d sesiones", transportCfg.port, transportCfg.port+(pjsua_call_get_max_count()*2)-1, pjsua_call_get_max_count()));

		

		/**
		 * Crea un control de acceso a la memoria del agente.
		 */
		st = pj_lock_create_recursive_mutex(pjsua_var.pool, NULL, &_Lock);
		PJ_CHECK_STATUS(st, ("ERROR creando seccion critica"));

		/**
		 * Crea mutex para acceso al cancelador de eco LC-mic.
		 */
		pj_status_t st = pj_lock_create_recursive_mutex(pjsua_var.pool, NULL, &_ECLCMic_mutex);
		PJ_CHECK_STATUS(st, ("ERROR creando _ECLCMic_mutex"));

		/** 
		* Crea módulo de suscripción WG67
		**/
		WG67Subscription::Init(NULL, cfg);

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

		if (_RecordPortTel != NULL && _RecordPortRad != NULL)
		{
			_RecordPortTel->SetTheOtherRec(_RecordPortRad);
			_RecordPortRad->SetTheOtherRec(_RecordPortTel);
		}

		_FrecDesp = new FrecDesp;

		_PresenceManager = new PresenceManag;	//Se inicializa la gestion de presencias
		_PresenceManager->SetPresenceSubscriptionCallBack(cfg->Cb.Presence_callback);

		_ConfManager = new SubsManager<ConfSubs>;			//se inicializa el gestor de subscripcion conferencias
		_DlgManager = new SubsManager<DlgSubs>;			//se inicializa el gestor de subscripcion conferencias

#ifdef CHECK_QIDX_LOGARITHM

		//Creamos un puerto de media que reproduzca un tono

		if (SipAgent::_Radio_UA)
		{			
			//Creamos un generador de tono para enviar cuando hay squelch
			st = pjmedia_tonegen_create(pjsua_var.pool, SAMPLING_RATE, CHANNEL_COUNT, SAMPLES_PER_FRAME, BITS_PER_SAMPLE, PJMEDIA_TONEGEN_LOOP, &_Tone_Port);
			if (st != PJ_SUCCESS)
			{
				PJ_LOG(3,(__FILE__, "ERROR: SipCall: No se puede crear _Tone_Port"));
				_Tone_Port = NULL;
			}
			else
			{
				st = pjsua_conf_add_port(pjsua_var.pool, _Tone_Port, &_Tone_Port_Id);
				if (st != PJ_SUCCESS)
				{
					PJ_LOG(3,(__FILE__, "ERROR: SipCall: pjsua_conf_add_port para obtener _Tone_Port_Id retorna error"));
					_Tone_Port_Id = PJSUA_INVALID_ID;
				}
				else
				{
					pjmedia_tone_desc tonegen_desc[1];

					tonegen_desc[0].freq1 = 1000;
					tonegen_desc[0].freq2 = 600;
					tonegen_desc[0].on_msec = 20;
					tonegen_desc[0].off_msec = 0;
					tonegen_desc[0].flags = 0;

					st = pjmedia_tonegen_play(_Tone_Port, 1, tonegen_desc, PJMEDIA_TONEGEN_LOOP);
					if (st != PJ_SUCCESS)
					{
						PJ_LOG(3,(__FILE__, "ERROR: SipCall: pjmedia_tonegen_play retorna error"));
					}						
				}
			}
		}
#endif

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

		//for (pjsua_call_id cid = 0; cid < ((pjsua_call_id) PJ_ARRAY_SIZE(pjsua_var.calls)); cid++)
		for (pjsua_call_id cid = 0; cid < ((pjsua_call_id) pjsua_call_get_max_count()); cid++)
		{
			SipCall::Force_Hangup(cid, 0);
		}
		//pjsua_call_hangup_all();

		/**
		 * Espera a que todas las llamadas se hayan cerrado.
		 */

		for (int i = 0; i < PJ_ARRAY_SIZE(pjsua_var.calls); ++i) 
		{
			if (pjsua_var.calls[i].inv != NULL) 
			{
				int cnt=0;
				while (cnt<10 && pjsua_var.calls[i].inv != NULL)
				{
					pj_thread_sleep(100);
					cnt++;
				}
				// goto CHECK_CALLS;
			}
		}


		/* Si el publish esta habilitado. He visto que el servidor de presencia
		tarda en contestar a veces mas de 400ms. en cuyo caso se destruye acc->publish_sess
		y cuando se cierra la aplicacion no envia el PUBLISH con expires a cero.
		Aqui me aseguro de que se va a enviar*/
		for (int i=0; i<PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
			pjsua_acc *acc = &pjsua_var.acc[i];
			if (acc->valid)
			{
				if (acc->cfg.publish_enabled && acc->publish_sess==NULL)
				{
					pjsua_acc_set_online_status((pjsua_acc_id) i, PJ_FALSE);	//Deja de publicarse en el servidor de presencia	
					pjsua_pres_init_publish_acc(acc->index);
				}
			}
		}

		if (_PresenceManager) 
		{
			delete _PresenceManager;
			_PresenceManager = NULL;
		}

		if (_ConfManager)
		{
			delete _ConfManager;
			_ConfManager = NULL;
		}

		if (_DlgManager)
		{
			delete _DlgManager;
			_DlgManager = NULL;
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

		//Libera FD
		if (_FrecDesp)
		{
			delete _FrecDesp;
			_FrecDesp = NULL;
		}

		/**
		 * Libera puerto de grabacion
		 */
		if (_RecordPortTel)
		{
			delete _RecordPortTel;
			_RecordPortTel = NULL;
		}

		if (_RecordPortRad)
		{
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

		if (_EchoCancellerLCMic != NULL)
		{
			EchoCancellerLCMic(false);
		}

		WG67Subscription::End();

		lock.Unlock();
		pj_lock_destroy(_Lock);

		pjsua_destroy();
	}
}

/**
 * SetSipPort.				Establece el puerto SIP
 * @param	port			Puerto SIP
 * @return	Nada
 */
void SipAgent::SetSipPort(unsigned int port)
{
	pj_status_t st = PJ_SUCCESS;

	st = pjsua_transport_close(SipTransportId, PJ_FALSE);
	PJ_CHECK_STATUS(st, ("ERROR cerrando SIP transport", " SipTransportId = %d ", SipTransportId));

	SipTransportId = PJSUA_INVALID_ID;

	pjsua_transport_config transportCfg;
	pjsua_transport_config_default(&transportCfg);
	transportCfg.port = port;
	transportCfg.bound_addr = pj_str(const_cast<char*>(SipAgent::uaIpAdd));
	transportCfg.options = PJMEDIA_UDP_NO_SRC_ADDR_CHECKING;

	st = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transportCfg, &SipTransportId);
	PJ_CHECK_STATUS(st, ("ERROR creando puertos UDP para Sip", "(%s:%d)", SipAgent::uaIpAdd, port));
}

/**
 * SetRTPPort.				Establece el primer puerto RTP para la media.
 * @param	port			Puerto SIP
 * @return	Nada
 */
void SipAgent::SetRTPPort(unsigned int port)
{
	pj_status_t st = PJ_SUCCESS;

	pjsua_transport_config transportCfg;
	pjsua_transport_config_default(&transportCfg);
	transportCfg.port = port;
	transportCfg.bound_addr = pj_str(const_cast<char*>(SipAgent::uaIpAdd));
	transportCfg.options = PJMEDIA_UDP_NO_SRC_ADDR_CHECKING;

	//La siguiente funcion crea los transports para el RTP. Internamente borra los que tuviera creados.
	//Crea un par de sockets (rtp/rtcp) para cada llamada. Por tanto en total crea pjsua_var.ua_cfg.max_calls*2
	st = pjsua_media_transports_create(&transportCfg);
	PJ_CHECK_STATUS(st, ("ERROR creando puertos UDP para RTP", "(%s:%d)", SipAgent::uaIpAdd, port));
}

/**
 * GetRTPPort.				Obtiene el primer puerto RTP/RTCP. Busca un hueco de puertos UDP libres en el S.O. 
 *							para todas las posibles llamadas que pueda utilizar la CORESIP.
 * @param					port. Valor del puerto obtenido
 * @return					-1 si a ocurrido algún error.
 */
int SipAgent::GetRTPPort(unsigned int *port)
{	
	int ret = 0;

	//Para cada llamada es necesario dos sockets, uno para rtp y otro para rtcp.
	//Por tanto buscamos en nuestro sistema operativo una cantidad de puertos consecutivos
	//igual a pjsua_var.ua_cfg.max_calls*2
	//Empezamos por los puertos altos. Concretamente desde el 64998u y hacia atrás. Dejamos el 65000 porque es el que se utiliza para la grabacion.
	
	int max_rtp_rtcp_ports = pjsua_call_get_max_count() * 2;			//pjsua_var.ua_cfg.max_calls*2
	
	PMIB_UDPTABLE pUdpTable;
	unsigned short *pUdpPorts;
	DWORD dwSize = 0;
	DWORD dwRetVal = 0;

	unsigned short *port_ptr;

	/* Get size required by GetUdpTable() */
	if (GetUdpTable(NULL, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) 
	{
		pUdpTable = (MIB_UDPTABLE *) malloc (dwSize);	
		if (pUdpTable == NULL)
		{
			PJ_CHECK_STATUS(PJ_ENOMEM, ("ERROR. GetUdpTable retorna con error. No se puede obtener la lista de puertos UDP utilizados"));				
			ret = -1;
			return ret;
		}
	}
	else
	{
		ret = -1;
	}

	unsigned short limite_superior = 64998u;
	unsigned short puerto_obtenido = -1;

	if (ret == 0)
	{
		/* Get actual data using GetUdpTable() */
		//Obtenemos todos los puertos que utiliza nuestro sistema operativo, ordenados de menor a mayor
		//Buscamos un hueco entre puertos utilizados donde podamos utilizar todos los puertos que requerimos. Es decir, max_rtp_rtcp_ports
		//Empezamos por el puerto mas alto ulilizado de la tabla obtenida y calculamos la diferencia con limite_superior.
		//Si la diferencia es mayor que la cantidad de puertos requeridos entonces ya tenemos hueco.
		//Si no hay hueco entonces limite_superior toma el valor del puerto de la tabla que hemos utilizado, así hasta obtener el hueco.	

		if ((dwRetVal = GetUdpTable(pUdpTable, &dwSize, 0)) == NO_ERROR) 
		{
			if (pUdpTable->dwNumEntries > 0) 
			{
				//Guardamos los puertos en un array
				pUdpPorts = (unsigned short *) malloc (pUdpTable->dwNumEntries * sizeof(unsigned short));
				if (pUdpPorts == NULL)
				{
					PJ_CHECK_STATUS(PJ_ENOMEM, ("ERROR. GetUdpTable retorna con error. No se puede obtener la lista de puertos UDP utilizados"));
					free(pUdpTable);
					ret = -1;
					return ret;
				}

				for (unsigned int i = 0; i < pUdpTable->dwNumEntries; i++)
				{
					port_ptr = (unsigned short *)&pUdpTable->table[i].dwLocalPort;
					unsigned short used_port = htons(*port_ptr);
					pUdpPorts[i] = used_port;
				}

				//Ordenamos el array de menor puerto a mayor
				for (unsigned int i = 0; i < pUdpTable->dwNumEntries-1; i++)
				{
					for (unsigned j = i+1; j < pUdpTable->dwNumEntries; j++)
					{				
						if (pUdpPorts[i] > pUdpPorts[j])
						{
							unsigned short aux = pUdpPorts[i];
							pUdpPorts[i] = pUdpPorts[j];
							pUdpPorts[j] = aux;
						}
					}
				}

				PJ_LOG(5,(__FILE__, "Puertos usados: #############################################"));
				for (unsigned int i = 0; i < pUdpTable->dwNumEntries; i++)
				{					
					PJ_LOG(5,(__FILE__, "Puerto usado: %u", pUdpPorts[i]));
				}
				PJ_LOG(5,(__FILE__, "#############################################"));
				
				for (unsigned int i = pUdpTable->dwNumEntries-1; i >= 0; i--)
				{			

					unsigned short nport = pUdpPorts[i];
					//Le sumanos 2 y nos aseguramos de que es par
					nport += 2;
					if ((nport % 2) != 0)
					{
						nport += 1;
					}
										
					if (nport < limite_superior)
					{
						//El valor tiene que ser menor que el de limite_superior, si no es asi no hacemos nada y tomaremos el siguiente mas bajo de la tabla

						if ((limite_superior - nport) > max_rtp_rtcp_ports)
						{
							//Hemos encontrado un hueco donde caben todos los puertos que necesitamos
							puerto_obtenido = limite_superior - max_rtp_rtcp_ports;
							break;
						}
						else
						{
							//Si no cabe entonces el limite superior cambia al del valor de la tabla que hemos utilizado
							limite_superior = pUdpPorts[i];

							//Si es impar le restamos 1
							if ((limite_superior % 2) != 0)
							{
								limite_superior -= 1;
							}
						}
					}					
				}

				free(pUdpPorts);

			}
			else
			{
				ret = -1;
			}
		} 
		else 
		{
			ret = -1;
		}

		free(pUdpTable);
	}

	if (ret == -1)
	{
		PJ_CHECK_STATUS(PJ_EUNKNOWN, ("ERROR. GetUdpTable retorna con error. No se puede obtener la lista de puertos UDP utilizados"));				
	}

	if (puerto_obtenido < 0)
	{
		//No hemos encontrado hueco
		PJ_CHECK_STATUS(PJ_EUNKNOWN, ("ERROR. No se encuentra una cantidad de puertos RTP/RTCP consecutivos suficientes en el S.O."));
		ret = -1;
	}
	else
	{
		*port = puerto_obtenido;
	}

	return ret;
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
 * @param	acc			Puntero a la URI que se crea como agente. 
 * @param	defaultAcc	Si es diferente a '0', indica que se creará la cuentra por Defecto.
 * @param	proxy_ip	Si es distinto de NULL. IP del proxy Donde se quieren enrutar los paquetes.
 * @return	Indentificador de la cuenta.
 */
int SipAgent::CreateAccount(const char * acc, int defaultAcc, const char *proxy_ip)
{
	pjsua_acc_config accCfg;
	pjsua_acc_config_default(&accCfg);
	pj_str_t sturi = pj_str(const_cast<char*>("sip:"));
	pj_str_t stproxip = pj_str(const_cast<char*>(proxy_ip));
	pjsua_acc_id accId = PJSUA_INVALID_ID;
	pj_status_t st;	
	
	accCfg.id = pj_str(const_cast<char*>(acc));

	//"sip:proxy_ip"
	char cproxy_route[128];
	if (proxy_ip != NULL)
	{
		if (strlen(proxy_ip) > 0)
		{
			if ((strlen(proxy_ip) + sturi.slen + 1) > sizeof(cproxy_route))
			{
				st = PJ_ENOMEM;
				PJ_CHECK_STATUS(st, ("ERROR creando usuario Sip. Memoria insuficiente", "(%s)", acc));
				return accId;
			}

			pj_bzero(cproxy_route, sizeof(cproxy_route));
			accCfg.proxy_cnt = 1;
			accCfg.proxy[0] = pj_str(cproxy_route);	
			pj_strcpy(&accCfg.proxy[0], &sturi);
			pj_strcat(&accCfg.proxy[0], &stproxip);

			accCfg.publish_enabled = PJ_TRUE;		//Se activa PUBLISH para un servidor de presencia
		}
	}	

	ExtraParamAccId *extraParamAccCfg = new ExtraParamAccId();
	accCfg.user_data = (void *) extraParamAccCfg;

#if 0
	//Generamos pidf_tuple_id para el PIDF ID de los PUBLISH y NOTIFIes
	char tuple_id_buf[PJ_GUID_MAX_LENGTH+2];
	pj_bzero(tuple_id_buf, sizeof(tuple_id_buf));
	accCfg.pidf_tuple_id = pj_str(tuple_id_buf);

    accCfg.pidf_tuple_id.ptr += 2;
	pj_generate_unique_string(&accCfg.pidf_tuple_id);
	accCfg.pidf_tuple_id.ptr -= 2;
	accCfg.pidf_tuple_id.ptr[0] = 'n';
	accCfg.pidf_tuple_id.ptr[1] = 'u';
	accCfg.pidf_tuple_id.slen += 2;
#endif

	st = pjsua_acc_add(&accCfg, defaultAcc, &accId);
	PJ_CHECK_STATUS(st, ("ERROR creando usuario Sip", "(%s)", acc));

	if (accCfg.publish_enabled == PJ_TRUE)
	{
		pjsua_acc_set_online_status(accId, PJ_TRUE);		//Inicia el envio de publish al servidor de presencia indicando que
															//está activo
		//pjsua_pres_update_acc(accId, PJ_TRUE);				//Forzamos la actualizacion del estado de presencia

		pjsua_pres_init_publish_acc(accId);

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
	}

	return accId;
}

/**
 * RegisterInProxy:		Crea una cuenta y se registra en el SIP proxy. Los paquetes sip se rutean por el SIP proxy también.
 * @param	acc			Puntero al Numero de Abonado (usuario). NO a la uri.
 * @param	defaultAcc	Si es diferente a '0', indica que se creará la cuenta por Defecto.
 * @param	proxy_ip	IP del proxy.
 * @param	expire_seg  Tiempo en el que expira el registro en segundos.
 * @param	username	Si no es necesario autenticación, este parametro será NULL
 * @param   pass		Password. Si no es necesario autenticación, este parametro será NULL
 * @param   displayName	friendly name to be displayed included before SIP uri
 * @param	isfocus		Si el valor es distinto de cero, indica que es Focus, para establecer llamadas multidestino
 * @return	Indentificador de la cuenta.
 */
int SipAgent::CreateAccountAndRegisterInProxy(const char * acc, int defaultAcc, const char *proxy_ip, unsigned int expire_seg, const char *username, const char *pass, const char *displayName, int isfocus)
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
	pj_str_t st_doubleQuote = pj_str(const_cast<char*>("\""));
	pj_str_t st_space = pj_str(const_cast<char*>(" "));
	pj_str_t st_menorque = pj_str(const_cast<char*>("<"));
	pj_str_t st_mayorque = pj_str(const_cast<char*>(">"));
	pj_str_t st_displayName = pj_str(const_cast<char*>(displayName));

	char uaport[16];
	pj_utoa(SipAgent::uaPort, uaport);	 
	pj_str_t stuaport = pj_str(const_cast<char*>(uaport));
			
	//"displayname" <sip:accID@user_agent_ip:5060>
	char cid[128];

	for (int i = 0; i < stacc.slen; i++)
		stacc.ptr[i] = pj_tolower(stacc.ptr[i]);	

	if ((pj_strlen(&st_doubleQuote)+pj_strlen(&st_displayName)+pj_strlen(&st_doubleQuote) + pj_strlen(&st_space) + 
		pj_strlen(&st_menorque) + pj_strlen(&sturi) + pj_strlen(&stacc) + pj_strlen(&starr) + pj_strlen(&st_uaIpAdd) + 
		pj_strlen(&stpp) + pj_strlen(&stuaport) +  pj_strlen(&st_mayorque) + 1) > sizeof(cid))
	{
		st = PJ_ENOMEM;
		PJ_CHECK_STATUS(st, ("ERROR creando usuario Sip. Memoria insuficiente", "(%s)", acc));
		return accId;
	}

	pj_bzero(cid, sizeof(cid));
	accCfg.id = pj_str(cid);
	pj_strcpy(&accCfg.id, &st_doubleQuote);
	pj_strcat(&accCfg.id, &st_displayName);
	pj_strcat(&accCfg.id, &st_doubleQuote);
	pj_strcat(&accCfg.id, &st_space);
	pj_strcat(&accCfg.id, &st_menorque);
	pj_strcat(&accCfg.id, &sturi);
	pj_strcat(&accCfg.id, &stacc);
	pj_strcat(&accCfg.id, &starr);
	pj_strcat(&accCfg.id, &pj_str(const_cast<char*>(uaIpAdd)));
	pj_strcat(&accCfg.id, &stpp);
	pj_strcat(&accCfg.id, &stuaport);
	pj_strcat(&accCfg.id, &st_mayorque);

	//"sip:proxy_ip"
	char cprox[128];

	if ((pj_strlen(&sturi) + pj_strlen(&stproxip) + 1) > sizeof(cprox))
	{
		st = PJ_ENOMEM;
		PJ_CHECK_STATUS(st, ("ERROR creando usuario Sip. Memoria insuficiente", "(%s)", acc));
		return accId;
	}

	pj_bzero(cprox, sizeof(cprox));
	accCfg.reg_uri = pj_str(cprox);
	pj_strcpy(&accCfg.reg_uri, &sturi);
	pj_strcat(&accCfg.reg_uri, &stproxip);
		
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


	//Agragamos la ip del proxy para que todos los sips lleven la cabecera route.
	char cproxy_route[128];	
	pj_bzero(cproxy_route, sizeof(cproxy_route));
	accCfg.proxy_cnt = 1;
	accCfg.proxy[0] = pj_str(cproxy_route);	
	pj_strcpy(&accCfg.proxy[0], &sturi);
	pj_strcat(&accCfg.proxy[0], &stproxip);

	if (isfocus)
	{
		accCfg.contact_params = pj_str(const_cast<char*>(";isfocus"));
	}

	accCfg.publish_enabled = PJ_TRUE;		//Se activa PUBLISH para un servidor de presencia

	ExtraParamAccId *extraParamAccCfg = new ExtraParamAccId();
	accCfg.user_data = (void *) extraParamAccCfg;

#if 0
	//Generamos pidf_tuple_id para el PIDF ID de los PUBLISH y NOTIFIes
	char tuple_id_buf[PJ_GUID_MAX_LENGTH+2];
	pj_bzero(tuple_id_buf, sizeof(tuple_id_buf));
	accCfg.pidf_tuple_id = pj_str(tuple_id_buf);

    accCfg.pidf_tuple_id.ptr += 2;
	pj_generate_unique_string(&accCfg.pidf_tuple_id);
	accCfg.pidf_tuple_id.ptr -= 2;
	accCfg.pidf_tuple_id.ptr[0] = 'n';
	accCfg.pidf_tuple_id.ptr[1] = 'u';
	accCfg.pidf_tuple_id.slen += 2;
#endif
	
	st = pjsua_acc_add(&accCfg, defaultAcc, &accId);
	PJ_CHECK_STATUS(st, ("ERROR creando usuario Sip", "(%s)", acc));

	if (accCfg.publish_enabled == PJ_TRUE)
	{
		pjsua_acc_set_online_status(accId, PJ_TRUE);		//Inicia el envio de publish al servidor de presencia indicando que
															//está activo
		//pjsua_pres_update_acc(accId, PJ_TRUE);				//Forzamos la actualizacion del estado de presencia

		pjsua_pres_init_publish_acc(accId);

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
		
	pjsua_acc_set_online_status((pjsua_acc_id) id, PJ_FALSE);	//Deja de publicarse en el servidor de presencia	

	//Envia mensaje REGISTER para eliminarse del servidor
	st = pjsua_acc_get_info((pjsua_acc_id) id, &info);
	if (info.has_registration)
	{
		//Esta cuenta esta registrada en servidor y se procede a eliminarla
		st = pjsua_acc_set_registration((pjsua_acc_id) id, PJ_FALSE);

		if (st == PJ_SUCCESS)
		{			
			//Espera a que el servidor responda un 200
			int tries = 20;
			do {
				pj_thread_sleep(5);
				st = pjsua_acc_get_info((pjsua_acc_id) id, &info);
				//PJ_CHECK_STATUS(st, ("ERROR pjsua_acc_get_info", "(%d)", id));
			} while ((info.status != PJSIP_SC_OK) && (--tries > 0) && (st == PJ_SUCCESS));	
			pj_thread_sleep(50);
		}

		//Necesario para que la funcion pjsua_acc_del no produzca una excepcion al intentar volver a
		//eliminar el registro en el servidor. Es un bug de la libreria
		if (pjsua_var.acc[id].regc) {
			pjsip_regc_destroy(pjsua_var.acc[id].regc);
		}
		pjsua_var.acc[id].regc = NULL;
	} 	

	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(id);
	if (extraParamAccCfg != NULL) delete extraParamAccCfg;

	st = pjsua_acc_del((pjsua_acc_id) id);
	PJ_CHECK_STATUS(st, ("ERROR eliminando PJSUA account", "(%d)", id));

	pj_thread_sleep(250);		//Se pone un retardo paradejar tiempo a que se desregistre la cuenta antes de poder volver
								//a registrar otra
	
	return;
}

/**
 *	SetTipoGRS. Configura el tipo de GRS. El ETM lo llama cuando crea un account tipo GRS.
 *	@param	accId		Identificador de la cuenta.
 *	@param	FlagGRS	Tipo de GRS.
 */
void SipAgent::SetTipoGRS(int id, CORESIP_CallFlags Flag)
{
	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(id);
	if (extraParamAccCfg != NULL)
	{		
		extraParamAccCfg->rdAccount = PJ_TRUE;		//Indicamos que este account es de una radio GRS
		extraParamAccCfg->TipoGrsFlags = Flag;

		SipAgent::_HaveRdAcc = PJ_TRUE;
	}
}

pjsua_acc_id SipAgent::GetTipoGRS(int id, CORESIP_CallFlags* pTipoGRS)
{
	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(id);
	if (extraParamAccCfg != NULL)
	{
		*pTipoGRS = extraParamAccCfg->TipoGrsFlags;
		return id;
	}	
	return PJSUA_INVALID_ID;
}

/**
 * AddSndDevice: Añade un dispositivo de sonido en la Tabla de dispositivos del modulo.
 * @param	info	Puntero a la Informacion del dispositivo de Sonido.
 * @return	Identificador del Dispositivo. 
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
	if (dev == PJ_ARRAY_SIZE(_SndPorts)) 
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("AddSndDevice: No es posible sumar mas dispositivos de sonido");
	}

	_SndPorts[dev] = new SoundPort(info->Type, info->OsInDeviceIndex, info->OsOutDeviceIndex);

	if (_SndPorts[dev] == NULL)
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("AddSndDevice: No se puede crear un nuevo SoundPort");
	}

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
	if (id == PJ_ARRAY_SIZE(_WavPlayers)) 
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("CreateWavPlayer: No es posible sumar mas reproductores wav");
	}

	/**
	 * Crea el Reproductor a través de la clase @ref WavPlayer.
	 */
	_WavPlayers[id] = new WavPlayer(file, PTIME, loop, OnWavPlayerEof, (void*)(size_t)id);

	if (_WavPlayers[id] == NULL)
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("CreateWavPlayer: No se puede crear un nuevo WavPlayer");
	}

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
	if (id == PJ_ARRAY_SIZE(_WavRecorders))
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("CreateWavRecorder: No se pueden sumar mas elementos _WavRecorders");
	}
	/**
	 * Crea el 'grabador' a través de la clase @ref WavRecorder
	 */
	_WavRecorders[id] = new WavRecorder(file);
	if (_WavRecorders[id] == NULL)
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("CreateWavRecorder: No se puede crear mas elementos _WavRecorders");
	}
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
	if (id == PJ_ARRAY_SIZE(_RdRxPorts))
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("CreateRdRxPort: No se pueden sumar mas elementos _RdRxPorts");
	}

	_RdRxPorts[id] = new RdRxPort(SAMPLING_RATE, CHANNEL_COUNT, BITS_PER_SAMPLE, PTIME,
		info->ClkRate, info->ChannelCount, info->BitsPerSample, info->FrameTime, localIp, info->Ip, info->Port);

	if (_RdRxPorts[id] == NULL)
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("CreateRdRxPort: No se puede crear mas elementos _RdRxPorts");
	}

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
	if (id == PJ_ARRAY_SIZE(_SndRxPorts))
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("CreateSndRxPort: No se pueden sumar mas elementos _SndRxPorts");
	}

	_SndRxPorts[id] = new SoundRxPort(name, SAMPLING_RATE, CHANNEL_COUNT, BITS_PER_SAMPLE, PTIME);

	if (_SndRxPorts[id] == NULL)
	{
		throw PJLibException(__FILE__, PJ_ETOOMANY).Msg("CreateSndRxPort: No se pueden crear mas elementos _SndRxPorts");
	}

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
 * EchoCancellerLCMic.	...
 * Activa/desactiva cancelador de eco altavoz LC y Microfonos. Sirve para el modo manos libres 
 * @param	on						true - activa / false - desactiva
 * @return	CORESIP_OK OK, CORESIP_ERROR  error.
 */
int SipAgent::EchoCancellerLCMic(bool on)
{
	pj_status_t st;
	int ret = CORESIP_OK;

	st = pj_lock_acquire(_ECLCMic_mutex);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: EchoCancellerLCMic: SipAgent::_ECLCMic_mutex No puede adquirirse"));
	}

	if (on)
	{
		if (_EchoCancellerLCMic == NULL)
		{
			/*Creamos un cancelador de eco para manos libres. En el caso de que el audio de telefonia
			salga por altavoz de manos libres se cancela el eco en los microfonos*/
			st = pjmedia_echo_create2(pjsua_var.pool, SAMPLING_RATE, CHANNEL_COUNT,
					  ((SAMPLING_RATE * CHANNEL_COUNT * PTIME) / 1000), //dn_port->info.samples_per_frame,
					  SipAgent::EchoTail, SipAgent::EchoLatency, 0 , &_EchoCancellerLCMic);
			if (st != PJ_SUCCESS) 
			{
				_EchoCancellerLCMic = NULL;	
				PJ_LOG(3,(__FILE__, "ERROR: SipAgent::EchoCancellerLCMic No puede crearse"));
				ret = CORESIP_ERROR;
				PJ_CHECK_STATUS(st, ("ERROR: SipAgent::EchoCancellerLCMic No puede crearse"));				
			}
		}		
	}
	else
	{
		if (_EchoCancellerLCMic != NULL)
		{
			pjmedia_echo_state *ec_aux = _EchoCancellerLCMic;
			_EchoCancellerLCMic = NULL;
			st = pjmedia_echo_destroy(ec_aux);
			if (st != PJ_SUCCESS) 
			{
				PJ_LOG(3,(__FILE__, "ERROR: SipAgent::EchoCancellerLCMic No puede ser destruido"));
				ret = CORESIP_ERROR;
				PJ_CHECK_STATUS(st, ("ERROR: SipAgent::EchoCancellerLCMic No puede ser destruido"));
			}
		}
	}

	st = pj_lock_release(_ECLCMic_mutex);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: EchoCancellerLCMic: SipAgent::_ECLCMic_mutex No puede liberarse"));
	}

	return ret;
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
 * Retorna Retorna el número de llamadas confirmadas (en curso) y que tienen media, es decir, no estan en hold
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
			if (info.state == PJSIP_INV_STATE_CONNECTING || info.state == PJSIP_INV_STATE_CONFIRMED) 
			{
				if (info.media_status != PJSUA_CALL_MEDIA_NONE && info.media_status != PJSUA_CALL_MEDIA_ERROR)
				{
					if (info.media_dir != PJMEDIA_DIR_NONE)
					{
						ret++;
					}
				}
			}
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
 * RecINVTel.	...
 * Envía el comando INV al modulo de grabacion para telefonia 
 * @return	0 OK, -1  error.
 */
int SipAgent::RecINVTel()
{
	if (!_RecordPortTel) return -1;
	return _RecordPortTel->RecINV();
}

/**
 * RecINVRad.	...
 * Envía el comando INV al modulo de grabacion para radio 
 * @return	0 OK, -1  error.
 */
int SipAgent::RecINVRad()
{
	if (!_RecordPortRad) return -1;
	return _RecordPortRad->RecINV();
}

/**
 * RecBYETel.	...
 * Envía el comando BYE al modulo de grabacion para telefonia 
 * @return	0 OK, -1  error.
 */
int SipAgent::RecBYETel()
{
	if (!_RecordPortTel) return -1;
	return _RecordPortTel->RecBYE();
}

/**
 * RecBYERad.	...
 * Envía el comando BYE al modulo de grabacion para radio
 * @return	0 OK, -1  error.
 */
int SipAgent::RecBYERad()
{
	if (!_RecordPortRad) return -1;
	return _RecordPortRad->RecBYE();
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
 *	RecorderCmd. Se pasan comandos para realizar acciones sobre el grabador VoIP
 *  @param  cmd			Comando
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
int SipAgent::RecorderCmd(CORESIP_RecCmdType cmd, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	switch (cmd)
	{
	case CORESIP_REC_RESET:
		if (_RecordPortTel != NULL) _RecordPortTel->RecReset();
		break;
	case CORESIP_REC_ENABLE:
		if (_RecordPortTel == NULL) 
		{
			_RecordPortTel = new RecordPort(RecordPort::TEL_RESOURCE, SipAgent::uaIpAdd, "127.0.0.1", 65003, SipAgent::HostId);
		}
		if (_RecordPortRad == NULL) 
		{
			_RecordPortRad = new RecordPort(RecordPort::RAD_RESOURCE, SipAgent::uaIpAdd, "127.0.0.1", 65003, SipAgent::HostId);	
		}
		if (_RecordPortTel != NULL && _RecordPortRad != NULL)
		{
			_RecordPortTel->SetTheOtherRec(_RecordPortRad);
			_RecordPortRad->SetTheOtherRec(_RecordPortTel);
		}
		break;
	case CORESIP_REC_DISABLE:
		if (_RecordPortTel != NULL)
		{
			RecordPort *_RecordPortTel_aux = _RecordPortTel;
			_RecordPortTel = NULL;
			delete _RecordPortTel_aux;
		}
		if (_RecordPortRad != NULL)
		{
			RecordPort *_RecordPortRad_aux = _RecordPortRad;
			_RecordPortRad = NULL;
			delete _RecordPortRad_aux;
		}
		break;
	default:
		ret = CORESIP_ERROR;
		if (error != NULL)
		{
			error->Code = ret;
			strcpy(error->File, __FILE__);
			sprintf(error->Info, "RecorderCmd: %d command is not valid", cmd);
		}
		break;
	}
		
	return ret;
}

/**
 *	RdPttEvent. Se llama cuando hay un evento de PTT
 *  @param  on			true=ON/false=OFF
 *	@param	freqId		Identificador de la frecuencia
 *  @param  dev			Indice del array _SndPorts. Es dispositivo (microfono) fuente del audio.
 *  @param  PTT_type	Tipo de PTT.
 */
void SipAgent::RdPttEvent(bool on, const char *freqId, int dev, CORESIP_PttType PTT_type)
{
	//Este evento de PTT lo llama la aplicacion del HMI cuando cambia el estado de PTT que nos retorna el transmisor
	//tendremos que tener en cuenta el estado del PTT local para dar por bueno este evento

	if (!_RecordPortRad) return;	

	if (on == false && SipAgent::PTT_local_activado == PJ_TRUE)
	{
		//Si el estado de ptt que nos envia la aplicacion es false pero el estado en local es true
		//entonces puede ser que el transmisor nos haya enviado por su cuenta un estado de ptt falso
		//y por tanto no lo vamos a tener en cuenta.
		return;
	}

	if (on == true && SipAgent::PTT_local_activado == PJ_FALSE)
	{
		//Si el estado de ptt que nos envia la aplicacion es true pero el estado en local es false
		//entonces puede ser que el transmisor nos haya enviado por su cuenta un estado de ptt falso
		//y por tanto no lo vamos a tener en cuenta ya que ese PTT no es nuestro.
		return;
	}

	_RecordPortRad->RecPTT(on, freqId, dev, PTT_type);
}

/**
 *	RdSquEvent. Se llama cuando hay un evento de SQUELCH
 *  @param  on			true=ON/false=OFF
 *  @param  dev			Indice del array _SndPorts. Es dispositivo (microfono) fuente del audio.
 *	@param	freqId		Identificador de la frecuencia
 *	@param	resourceId  Identificador del recurso seleccionado en el bss
 *	@param	bssMethod	Método bss
 *	@param  bssQidx		Indice de calidad
 */
void SipAgent::RdSquEvent(bool on, const char *freqId, const char *resourceId, const char *bssMethod, unsigned int bssQidx)
{
	if (!_RecordPortRad) return;
	_RecordPortRad->RecSQU(on, freqId, resourceId, bssMethod, bssQidx);
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
 * @return	Codigo de error 
 */
void SipAgent::BridgeLink(int srcType, int src, int dstType, int dst, bool on)
{
	pjsua_conf_port_id conf_src = PJSUA_INVALID_ID, conf_dst = PJSUA_INVALID_ID;
	pj_bool_t error_src = PJ_FALSE;
	pj_bool_t error_dst = PJ_FALSE;

	if (src < 0 || dst < 0)
	{
		//throw PJLibException(__FILE__, PJ_EINVAL).Msg("BridgeLink:", "src %d y dst %d deben ser mayores o iguales a 0", src, dst);		
		return;
	}

	Guard lock(_Lock);

	/**
	 * Obtiene el Puerto de Conferencia asociado al origen.
	 */
	switch (srcType)
	{
	case CORESIP_CALL_ID:
		conf_src = pjsua_call_get_conf_port(src);
		if (conf_src == PJSUA_INVALID_ID) 
		{
			if (!on)
			{
				PJ_LOG(3,(__FILE__, "WARNING: BridgeLink: call_id %d invalido. Posiblemente intentando desconectar audio de una llamada ya finalizada", src));
			}
			else
			{
				PJ_LOG(3,(__FILE__, "ERROR: BridgeLink: call_id %d invalido. Intentando conectar audio de una llamada", src));
			}
			error_src = PJ_TRUE;
		}
		break;
	case CORESIP_SNDDEV_ID:
		if (src >= CORESIP_MAX_SOUND_DEVICES) error_src = PJ_TRUE;
		else if (_SndPorts[src] == NULL) error_src = PJ_TRUE;
		else if (!IsSlotValid(_SndPorts[src]->Slot)) error_src = PJ_TRUE;
		else conf_src = _SndPorts[src]->Slot;
		break;
	case CORESIP_WAVPLAYER_ID:
		if (src >= CORESIP_MAX_WAV_PLAYERS) error_src = PJ_TRUE;
		else if (_WavPlayers[src] == NULL) error_src = PJ_TRUE;
		else if (!IsSlotValid(_WavPlayers[src]->Slot)) error_src = PJ_TRUE;
		else conf_src = _WavPlayers[src]->Slot;
		break;
	case CORESIP_RDRXPORT_ID:
		if (src >= CORESIP_MAX_RDRX_PORTS) error_src = PJ_TRUE;
		else if (_RdRxPorts[src] == NULL) error_src = PJ_TRUE;
		else if (!IsSlotValid(_RdRxPorts[src]->Slot)) error_src = PJ_TRUE;
		else conf_src = _RdRxPorts[src]->Slot;
		break;
	case CORESIP_SNDRXPORT_ID:
		if ((src & 0x0000FFFF) >= CORESIP_MAX_SOUND_RX_PORTS) error_src = PJ_TRUE;
		else if (_SndRxPorts[src & 0x0000FFFF] == NULL) error_src = PJ_TRUE;
		else if (!IsSlotValid(_SndRxPorts[src & 0x0000FFFF]->Slots[src >> 16])) error_src = PJ_TRUE;
		else conf_src = _SndRxPorts[src & 0x0000FFFF]->Slots[src >> 16];
		break;
	default:
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("BridgeLink:", "Tipo de Puerto de origen srcType 0x%08x no es valido", srcType);		
	}

	/**
	 * Obtiene el Puerto de Conferencia asociado al Destino.
	 */
	switch (dstType)
	{
	case CORESIP_CALL_ID:

		conf_dst = pjsua_call_get_conf_port(dst);
		if (conf_dst == PJSUA_INVALID_ID) 
		{
			if (!on)
			{
				PJ_LOG(3,(__FILE__, "WARNING: BridgeLink: call_id %d invalido. Posiblemente intentando desconectar audio de una llamada ya finalizada", src));
			}
			else
			{
				PJ_LOG(3,(__FILE__, "ERROR: BridgeLink: call_id %d invalido. Intentando conectar audio de una llamada", src));
			}
			error_dst = PJ_TRUE;
		}

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
		if (dst >= CORESIP_MAX_SOUND_DEVICES) error_dst = PJ_TRUE;
		else if (_SndPorts[dst] == NULL) error_dst = PJ_TRUE;
		else if (!IsSlotValid(_SndPorts[dst]->Slot)) error_dst = PJ_TRUE;
		else 
		{
			conf_dst = _SndPorts[dst]->Slot;

			if (_SndPorts[dst]->_Type == CORESIP_SND_LC_SPEAKER)
			{
				//Cada vez que se conecta o desconecta el audio de los altavoces LC reiniciamos el
				//cancelador de eco

				pj_status_t st = pj_lock_acquire(_ECLCMic_mutex);
				if (st != PJ_SUCCESS)
				{
					PJ_LOG(3,(__FILE__, "ERROR: BridgeLink: SipAgent::_ECLCMic_mutex No puede adquirirse"));
				}

				if (on) _AltavozLCActivado = PJ_TRUE;
				else _AltavozLCActivado = PJ_FALSE;
			
				if (SipAgent::_EchoCancellerLCMic != NULL)
				{					
					st = pjmedia_echo_reset(SipAgent::_EchoCancellerLCMic);
					if (st != PJ_SUCCESS)
					{
						PJ_LOG(3,(__FILE__, "ERROR: Reseteando el cancelador de eco"));
					}
				}
			
				st = pj_lock_release(_ECLCMic_mutex);
				if (st != PJ_SUCCESS)
				{
					PJ_LOG(3,(__FILE__, "ERROR: BridgeLink: SipAgent::_ECLCMic_mutex No puede liberarse"));
				}
			}
		}

		if (srcType == CORESIP_CALL_ID)
		{
			//Si el origen es una llamada telefónica lo añadimos o quitamos del array SlotsToSndPorts
			//del puerto de grabacion de telefonia
			if (on && _RecordPortTel != NULL && !error_src) 
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
			if (on && _RecordPortRad != NULL && !error_src) 
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
		if (dst >= CORESIP_MAX_WAV_RECORDERS) error_dst = PJ_TRUE;
		else if (_WavRecorders[dst] == NULL) error_dst = PJ_TRUE;
		else if (!IsSlotValid(_WavRecorders[dst]->Slot)) error_dst = PJ_TRUE;
		else conf_dst = _WavRecorders[dst]->Slot;
		break;
	default:
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("BridgeLink:", "Tipo de Puerto de destino dstType 0x%08x no es valido", dstType);		
	}

	if (error_src || error_dst) return;		//Alguno de los slots no es valido. Por tanto no seguimos.

	/**
	 * Conecta o Desconecta los puertos.
	 */
	if (on)
	{
		pj_status_t st = pjsua_conf_connect(conf_src, conf_dst);
		//PJ_CHECK_STATUS(st, ("ERROR conectando puertos", "(%d --> %d)", conf_src, conf_dst));
		if (st != PJ_SUCCESS)
		{
			throw PJLibException(__FILE__, st).Msg("BridgeLink:", "ERROR conectando puertos %d --> %d", conf_src, conf_dst);		
		}
	}
	else
	{
		pj_status_t st = pjsua_conf_disconnect(conf_src, conf_dst);
		if (st != PJ_SUCCESS)
		{
			throw PJLibException(__FILE__, st).Msg("BridgeLink:", "ERROR desconectando puertos %d --> %d", conf_src, conf_dst);		
		}
		//PJ_CHECK_STATUS(st, ("ERROR desconectando puertos", "(%d --> %d)", conf_src, conf_dst));
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
		SipAgent::PTT_local_activado = on;			//Esta funcion se llama cuando hay un PTT local en el puesto. Esta variable 
													//guarda el estado del PTT local
		if (_RecordPortRad != NULL)
		{
			//Se conecta el puerto de audio TX (microfono) pasado por dev al puerto de grabacion
			SipAgent::RecConnectSndPort(on, dev, _RecordPortRad);
		}
	}
	else
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("SendToRemote:", "Primer parametro typeDev no valido");
	}
}

/**
 * ReceiveFromRemote:	Configura el múdulo para recibir audio por Multicast. En Nodebox se recibe de los puestos cuando hay PTT
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
	pjsua_conf_port_id conf_id = PJSUA_INVALID_ID;

	if (id < 0)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("SetVolume:", "Segundo parametro (id=%d) debe ser mayor o igual a 0", id);		
	}

	/**
	 * Obtiene el Puerto de Conferencia asociado al origen.
	 */
	switch (idType)
	{
	case CORESIP_CALL_ID:
		conf_id = pjsua_call_get_conf_port(id);
		break;
	case CORESIP_SNDDEV_ID:
		if (id >= CORESIP_MAX_SOUND_DEVICES) break;
		if (_SndPorts[id] == NULL) break;
		if (!IsSlotValid(_SndPorts[id]->Slot)) break;
		conf_id = _SndPorts[id]->Slot;
		break;
	case CORESIP_WAVPLAYER_ID:
		if (id >= CORESIP_MAX_WAV_PLAYERS) break;
		if (_WavPlayers[id] == NULL) break;
		if (!IsSlotValid(_WavPlayers[id]->Slot)) break;
		conf_id = _WavPlayers[id]->Slot;
		break;
	case CORESIP_RDRXPORT_ID:
		if (id >= CORESIP_MAX_RDRX_PORTS) break;
		if (_RdRxPorts[id] == NULL) break;
		if (!IsSlotValid(_RdRxPorts[id]->Slot)) break;
		conf_id = _RdRxPorts[id]->Slot;
		break;
	case CORESIP_SNDRXPORT_ID:
		if ((id & 0x0000FFFF) >= CORESIP_MAX_SOUND_RX_PORTS) break;
		if (_SndRxPorts[id & 0x0000FFFF] == NULL) break;
		if (!IsSlotValid(_SndRxPorts[id & 0x0000FFFF]->Slots[id >> 16])) break;
		conf_id = _SndRxPorts[id & 0x0000FFFF]->Slots[id >> 16];
		break;
	default:
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("SetVolume:", "Primer parametro (Tipo dispositivo 0x%08x) no valido", idType);
	}

	if (conf_id == PJSUA_INVALID_ID)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("SetVolume:", "Segundo parametro (id=%d) no valido", id);
	}

	/**
	 * Si no hay error ajusta el volumen.
	 */
	pj_status_t st = pjmedia_conf_adjust_rx_level(pjsua_var.mconf, conf_id, (256 * ((int)volume - 50)) / 100);
	PJ_CHECK_STATUS(st, ("SetVolume:", "ERROR ajustando volumen idType=%d, id=%d, volume=%u", idType, id, volume));
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
	pjsua_conf_port_id conf_id = PJSUA_INVALID_ID;

	if (id < 0)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("GetVolume:", "Segundo parametro (id=%d) debe ser mayor o igual a 0", id);		
	}

	/**
	 * Obtiene el Puerto de Conferencia asociado al origen.
	 */
	switch (idType)
	{
	case CORESIP_CALL_ID:
		conf_id = pjsua_call_get_conf_port(id);
		break;
	case CORESIP_SNDDEV_ID:
		if (id >= CORESIP_MAX_SOUND_DEVICES) break;
		if (_SndPorts[id] == NULL) break;
		if (!IsSlotValid(_SndPorts[id]->Slot)) break;
		conf_id = _SndPorts[id]->Slot;
		break;
	case CORESIP_WAVPLAYER_ID:
		if (id >= CORESIP_MAX_WAV_PLAYERS) break;
		if (_WavPlayers[id] == NULL) break;
		if (!IsSlotValid(_WavPlayers[id]->Slot)) break;
		conf_id = _WavPlayers[id]->Slot;
		break;
	case CORESIP_RDRXPORT_ID:
		if (id >= CORESIP_MAX_RDRX_PORTS) break;
		if (_RdRxPorts[id] == NULL) break;
		if (!IsSlotValid(_RdRxPorts[id]->Slot)) break;
		conf_id = _RdRxPorts[id]->Slot;
		break;
	case CORESIP_SNDRXPORT_ID:
		if ((id & 0x0000FFFF) >= CORESIP_MAX_SOUND_RX_PORTS) break;
		if (_SndRxPorts[id & 0x0000FFFF] == NULL) break;
		if (!IsSlotValid(_SndRxPorts[id & 0x0000FFFF]->Slots[id >> 16])) break;
		conf_id = _SndRxPorts[id & 0x0000FFFF]->Slots[id >> 16];
		break;
	default:
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("GetVolume:", "Primer parametro (Tipo dispositivo 0x%08x) no valido", idType);
	}

	if (conf_id == PJSUA_INVALID_ID)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("GetVolume:", "Segundo parametro (id=%d) no valido", id);
	}

	/**
	 * Si no hay error, devuelve un valor entre 0 y 100.
	 */
	pjmedia_conf_port_info info;
	pj_status_t st = pjmedia_conf_get_port_info(pjsua_var.mconf, conf_id, &info);
	PJ_CHECK_STATUS(st, ("GetVolume:", "ERROR tomando volumen idType=%d, id=%d", idType, id));

	return (int)((info.rx_adj_level + 128) / 2,56);
}

/**
 * OnRxResponse:	Callback. Se invoca cuando se recibe una request SIP
 *						- Cuando corresponde a un metodo 'subscribe' no asociado a un invite. No lleva to-tag
 * @param	rdata		Puntero 'pjsip_rx_data' a los datos recibidos.
 * @return	'pj_bool_t' Siempre true si tiene exito.
 *
 */
pj_bool_t SipAgent::OnRxRequest(pjsip_rx_data *rdata)
{
	pjsip_method *req_method = &rdata->msg_info.msg->line.req.method;	

	if (
		(rdata->msg_info.to->tag.slen == 0) &&
		(rdata->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD) &&
		(pjsip_method_cmp(req_method, pjsip_get_subscribe_method()) == 0)
		)
	{
		pj_str_t STR_EVENT = { "Event", 5 };
		pj_str_t STR_CONFERENCE = { "conference", 10 };
		pj_str_t STR_DIALOG = { "dialog", 6 };
		//Hemos recibido una subscripcion sin to-tag
		pjsip_event_hdr * event = (pjsip_event_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_EVENT, NULL);
		pjsip_expires_hdr *expires = (pjsip_expires_hdr*)pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_EXPIRES, NULL);
		pjsip_to_hdr* to = (pjsip_to_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_TO, NULL);
		pjsip_from_hdr* from = (pjsip_from_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_FROM, NULL);
		if (to == NULL || from == NULL) return PJ_FALSE;

		//Buscamos el acc al que va dirigido
		pjsua_acc_id accid = pjsua_acc_find_for_incoming_by_uri(to->uri);
		if (accid == PJSUA_INVALID_ID) return PJ_FALSE;

		//Comprobamos si esta solicitud de subscripcion subscripcion ya ha sido descartada
		if (ExtraParamAccId::IsInDeletedList(accid, from->uri, &from->tag) == PJ_TRUE)
		{
			//Ya ha sido descartado. No hacemos nada
			return PJ_TRUE;
		}

		if (event && (pj_stricmp(&event->event_type, &STR_CONFERENCE) == 0))
		{
			if (expires->ivalue != 0)
			{
				//Hemos recibido una subscripcion al evento de conferencia sin to-tag. O sea fuera de dialogo INV.				

				pj_pool_t *tmppool = pjsua_pool_create(NULL, 64, 32);
				PJ_ASSERT_RETURN(tmppool != NULL, PJ_FALSE);
				pj_str_t contact;
				pj_status_t st = pjsua_acc_create_uas_contact(tmppool, &contact, accid, rdata);				
				if (st != PJ_SUCCESS)
				{
					PJ_LOG(3,(__FILE__, "ERROR: SipAgent::OnRxRequest: No se puede generar contact")); 
					pj_pool_release(tmppool);
					return PJ_FALSE;
				}
				
				pjsip_dialog *dlg;
				pjsip_evsub *_ConfsubMod;
				st = pjsip_dlg_create_uas( pjsip_ua_instance(), rdata, &contact, &dlg);
				pj_pool_release(tmppool);
				if (st == PJ_SUCCESS) 
				{
					pjsip_dlg_inc_lock(dlg);
					pjsip_evsub *conftmp = ExtraParamAccId::Get_subMod(accid, dlg->remote.info->uri, &STR_CONFERENCE);
					if (conftmp != NULL)
					{
						//El usuario ya está subscrito.
						ExtraParamAccId::Del_subMod(conftmp);
						pjsip_conf_terminate(conftmp, PJ_FALSE);		//Eliminamos la subscripcion antigua y nos quedamos con la nueva																				
						ExtraParamAccId::Add_DeletedsubModlist(accid, dlg->remote.info->uri, &dlg->remote.info->tag);
																		//Se agrega a la lista de borrados
					}

					st = pjsip_conf_create_uas(dlg, &ConfSubs::_ConfSrvCb, rdata, &_ConfsubMod);
					pjsip_dlg_dec_lock(dlg);
					if (st == PJ_SUCCESS)
					{
						pj_pool_t *pool = pjsip_evsub_get_pool(_ConfsubMod);
						ExtraParamAccId::subs_user_data * sub_user_data = (ExtraParamAccId::subs_user_data *) pj_pool_alloc(pool, sizeof(ExtraParamAccId::subs_user_data));
						PJ_ASSERT_RETURN(sub_user_data != NULL, PJ_FALSE);						
						sub_user_data->accid = accid;
						sub_user_data->send_notify_at_tsx_terminate = PJ_FALSE;
						pjsip_evsub_set_user_data(_ConfsubMod, (void *) sub_user_data);

						st = pjsip_conf_accept(_ConfsubMod, rdata, PJSIP_SC_OK, NULL);
						if (st == PJ_SUCCESS)
						{
							/*
								* He visto que cuando se crea el servidor a la subscripcion de conferencia no se arranca el timeout
								* que vence cuando expire time se cumple. Sin embargo cuando se reciben los subscribes de refresco sí.
								* Por tanto creo esta funcion para arrancar el timer.
								*/
							set_timer_uas_timeout(_ConfsubMod);

							PJ_LOG(5, ("SipAgent.cpp", "SipAgent::OnRxRequest conference sub %p creado con exito", _ConfsubMod));
							pjsip_evsub_set_state(_ConfsubMod, PJSIP_EVSUB_STATE_ACCEPTED);	
								
							/*Agregamos a la lista de subscripciones al evento de conferencia del account correspondiente*/
							ExtraParamAccId::Add_subMod(_ConfsubMod);
																
							if (SipAgent::Cb.IncomingSubscribeConfCb)
							{
								//Se notifica a la aplicacion que se ha aceptado la subscripcion al evento de conferencia
								//para que envie el notify con el estado
								char from_urich[CORESIP_MAX_URI_LENGTH];
								int size = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, from->uri, from_urich, CORESIP_MAX_URI_LENGTH-1);
								from_urich[size] = '\0';
								SipAgent::Cb.IncomingSubscribeConfCb(accid | CORESIP_ACC_ID, (const char *) from_urich, (const int) size);
							}

							return PJ_TRUE;
						}
					}
				}
			}
			else
			{
			}
		}
		else if (event && (pj_stricmp(&event->event_type, &STR_DIALOG) == 0))
		{
			//Se ha recibido una subscripcion al evento de dialogo 
			pj_pool_t *tmppool = pjsua_pool_create(NULL, 64, 32);
			PJ_ASSERT_RETURN(tmppool != NULL, PJ_FALSE);
			pj_str_t contact;
			pj_status_t st = pjsua_acc_create_uas_contact(tmppool, &contact, accid, rdata);			
			if (st != PJ_SUCCESS)
			{
				PJ_LOG(3,(__FILE__, "ERROR: SipAgent::OnRxRequest: No se puede generar contact")); 
				pj_pool_release(tmppool);
				return PJ_FALSE;
			}
			pjsip_dialog *dlg;
			pjsip_evsub *_DlgsubMod;
			st = pjsip_dlg_create_uas( pjsip_ua_instance(), rdata, &contact, &dlg);
			pj_pool_release(tmppool);
			if (st == PJ_SUCCESS) 
			{
				pjsip_dlg_inc_lock(dlg);
				pjsip_evsub *conftmp = ExtraParamAccId::Get_subMod(accid, dlg->remote.info->uri, &STR_DIALOG);
				if (conftmp != NULL)
				{
					//El usuario ya está subscrito.
					ExtraParamAccId::Del_subMod(conftmp);
					pjsip_evsub_terminate(conftmp, PJ_FALSE);		//Eliminamos la subscripcion antigua y nos quedamos con la nueva																				
					ExtraParamAccId::Add_DeletedsubModlist(accid, dlg->remote.info->uri, &dlg->remote.info->tag);
																	//Se agrega a la lista de borrados
				}

				st = pjsip_dialog_create_uas(dlg, &DlgSubs::_DlgSrvCb, rdata, &_DlgsubMod);
				pjsip_dlg_dec_lock(dlg);
				if (st == PJ_SUCCESS)
				{
					pj_pool_t *pool = pjsip_evsub_get_pool(_DlgsubMod);
					ExtraParamAccId::subs_user_data * sub_user_data = (ExtraParamAccId::subs_user_data *) pj_pool_alloc(pool, sizeof(ExtraParamAccId::subs_user_data));
					PJ_ASSERT_RETURN(sub_user_data != NULL, PJ_FALSE);
					sub_user_data->accid = accid;
					sub_user_data->send_notify_at_tsx_terminate = PJ_FALSE;
					pjsip_evsub_set_user_data(_DlgsubMod, (void *) sub_user_data);

					st = pjsip_dialog_accept(_DlgsubMod, rdata, PJSIP_SC_OK, NULL);
					if (st == PJ_SUCCESS)
					{
						if (expires->ivalue != 0)
						{
							/*
								* He visto que cuando se crea el servidor a la subscripcion  no se arranca el timeout
								* que vence cuando expire time se cumple. Sin embargo cuando se reciben los subscribes de refresco sí.
								* Por tanto creo esta funcion para arrancar el timer.
								*/
							set_timer_uas_timeout(_DlgsubMod);

							PJ_LOG(5, ("SipAgent.cpp", "SipAgent::OnRxRequest dialog sub %p creado con exito", _DlgsubMod));
							pjsip_evsub_set_state(_DlgsubMod, PJSIP_EVSUB_STATE_ACTIVE);	
								
							/*Agregamos a la lista de subscripciones al evento de conferencia del account correspondiente*/
							ExtraParamAccId::Add_subMod(_DlgsubMod);							
						}
						else
						{
							/*En este caso recibimos un subscribe con expires a cero para enviar el notify con el estado
							de dialogo. Por tanto no lo almacenamos en las lista de subscripciones*/
							PJ_LOG(5, ("SipAgent.cpp", "SipAgent::OnRxRequest dialog sub %p creado con exito expires a cero", _DlgsubMod));
							pjsip_evsub_set_state(_DlgsubMod, PJSIP_EVSUB_STATE_ACCEPTED);
						}
						sub_user_data->send_notify_at_tsx_terminate = PJ_TRUE;
				
						return PJ_TRUE;
					}
				}
			}
		}
	}
	return PJ_FALSE;
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
		char supported[CORESIP_MAX_SUPPORTED_LENGTH + 1] = { 0 };
		char allow[CORESIP_MAX_SUPPORTED_LENGTH + 1] = { 0 };
		char callid[CORESIP_MAX_CALLID_LENGTH + 1] = { 0 };

		if (rdata->msg_info.cid != NULL)
		{
			if (rdata->msg_info.cid->id.slen < sizeof(callid))
			{
				strncpy(callid, rdata->msg_info.cid->id.ptr, rdata->msg_info.cid->id.slen);
			}
		}

		pjsip_msg *msg = rdata->msg_info.msg;

		pjsip_sip_uri * dst = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.to->uri);

		if (dst->user.slen > 0)
		{
			//La uri destino del OPTIONS tiene usuario
			pj_ansi_snprintf(dstUri, sizeof(dstUri) - 1, "<sip:%.*s@%.*s>", 
				dst->user.slen, dst->user.ptr, dst->host.slen, dst->host.ptr);
		}
		else
		{
			//La uri destino del OPTIONS no tiene usuario
			pj_ansi_snprintf(dstUri, sizeof(dstUri) - 1, "<sip:%.*s>", 
				dst->host.slen, dst->host.ptr);
		}

		if (rdata->msg_info.supported != NULL)
		{		
			for (unsigned int i = 0; i < rdata->msg_info.supported->count; i++)
			{
				char method[64];
				if ((rdata->msg_info.supported->values[i].slen+2) < sizeof(method))
				{
					strncpy(method, rdata->msg_info.supported->values[i].ptr, rdata->msg_info.supported->values[i].slen);
					method[rdata->msg_info.supported->values[i].slen] = '\0';
					strcat(method, ", ");				

					if ((sizeof(supported) - (strlen(supported)+1)) > strlen(method))
					{
						strcat(supported, method);
					}
				}
			}
		}

		pjsip_allow_hdr *allow_hdr = (pjsip_allow_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_ALLOW, NULL);
		if (allow_hdr != NULL)
		{
			for (unsigned int i = 0; i < allow_hdr->count; i++)
			{
				char method[64];
				if ((allow_hdr->values[i].slen+2) < sizeof(method))
				{
					strncpy(method, allow_hdr->values[i].ptr, allow_hdr->values[i].slen);
					method[allow_hdr->values[i].slen] = '\0';
					strcat(method, ", ");				

					if ((sizeof(allow) - (strlen(allow)+1)) > strlen(method))
					{
						strcat(allow, method);
					}
				}
			}
		}

		if (strlen(supported) > 1)
			supported[strlen(supported)-2] = '\0';  //Se quita la ultima coma y el ultimo espacio
		if (strlen(allow) > 1)
			allow[strlen(allow)-2] = '\0';  //Se quita la ultima coma y el ultimo espacio

		if (Cb.OptionsReceiveCb)
		{
			Cb.OptionsReceiveCb(dstUri, callid, msg->line.status.code, supported, allow);
		}
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
	if (Cb.FinWavCb) Cb.FinWavCb((int)(size_t)userData | CORESIP_WAVPLAYER_ID);
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
			//pj_uint16_t *data_uint16 = (pj_uint16_t *) pl->Data;
			//PJ_LOG(3,(__FILE__, "INCIPALMA: OnDataReceived %d %s", *data_uint16, pl->SrcId));

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

int SipAgent::CreatePresenceSubscription(char *dest_uri)
{
	int ret = CORESIP_OK;

	if (_PresenceManager)
	{
		if (_PresenceManager->Add(dest_uri) != 0) ret = CORESIP_ERROR;
	}
	else
	{
		ret = CORESIP_ERROR;
		PJ_CHECK_STATUS(-1, ("ERROR: SipAgent::CreatePresenceSubscription: _PresenceManager no inicializado"));		
	}

	return ret;
}

int SipAgent::DestroyPresenceSubscription(char *dest_uri)
{
	int ret = CORESIP_OK;

	if (_PresenceManager)
	{
		if (_PresenceManager->Remove(dest_uri) != 0) ret = CORESIP_ERROR;
	}
	else
	{
		ret = CORESIP_ERROR;
		PJ_CHECK_STATUS(-1, ("ERROR: SipAgent::CreatePresenceSubscription: _PresenceManager no inicializado"));		
	}

	return ret;
}

int SipAgent::CreateConferenceSubscription(int acc_id, char *dest_uri, pj_bool_t by_proxy)
{
	int ret = CORESIP_OK;

	if (_ConfManager)
	{
		if (_ConfManager->Remove(dest_uri) != 0) ret = CORESIP_ERROR;
#ifdef _DEBUG
		else if (_ConfManager->Add(acc_id, dest_uri, 60, by_proxy) != 0) ret = CORESIP_ERROR;
#else
		else if (_ConfManager->Add(acc_id, dest_uri, -1, by_proxy) != 0) ret = CORESIP_ERROR;
#endif
	}
	else
	{
		ret = CORESIP_ERROR;
		PJ_CHECK_STATUS(-1, ("ERROR: SipAgent::CreateConferenceSubscription: _ConfManager no inicializado"));		
	}

	return ret;
}

int SipAgent::DestroyConferenceSubscription(char *dest_uri)
{
	int ret = CORESIP_OK;

	if (_ConfManager)
	{
		if (_ConfManager->Remove(dest_uri) != 0) ret = CORESIP_ERROR;
	}
	else
	{
		ret = CORESIP_ERROR;
		PJ_CHECK_STATUS(-1, ("ERROR: SipAgent::DestroyConferenceSubscription: _ConfManager no inicializado"));		
	}

	return ret;
}

int SipAgent::CreateDialogSubscription(int acc_id, char *dest_uri, pj_bool_t by_proxy)
{
	int ret = CORESIP_OK;

	if (_DlgManager)
	{
		if (_DlgManager->Remove(dest_uri) != 0) ret = CORESIP_ERROR;
#ifdef _DEBUG
		else if (_DlgManager->Add(acc_id, dest_uri, 60, by_proxy) != 0) ret = CORESIP_ERROR;
#else
		else if (_DlgManager->Add(acc_id, dest_uri, -1, by_proxy) != 0) ret = CORESIP_ERROR;
#endif
	}
	else
	{
		ret = CORESIP_ERROR;
		PJ_CHECK_STATUS(-1, ("ERROR: SipAgent::CreateDialogSubscription: _DlgManager no inicializado"));		
	}

	return ret;
}

int SipAgent::DestroyDialogSubscription(char *dest_uri)
{
	int ret = CORESIP_OK;

	if (_DlgManager)
	{
		if (_DlgManager->Remove(dest_uri) != 0) ret = CORESIP_ERROR;
	}
	else
	{
		ret = CORESIP_ERROR;
		PJ_CHECK_STATUS(-1, ("ERROR: SipAgent::DestroyDialogSubscription: _DlgManager no inicializado"));		
	}

	return ret;
}

/**
 * SendInstantMessage. Envia un mensaje instantaneo
 *
 * @param	acc_id		Account ID to be used to send the request.
 * @param	dest_uri	Uri del destino del mensaje. Acabado en 0.
 * @param	text		Texto plano a enviar. Acabado en 0
 * @param	by_proxy	Si es true el mensaje se envia por el proxy
 * @return	PJ_SUCCESS
 *
 */
int SipAgent::SendInstantMessage(int acc_id, char *dest_uri, char *text, pj_bool_t by_proxy)
{	
	pj_status_t st;
	if (by_proxy)
	{
		st = pjsua_im_send(acc_id, &pj_str(dest_uri), NULL,  &pj_str(text), NULL, NULL);
	}
	else
	{
		st = pjsua_im_send_no_proxy(acc_id, &pj_str(dest_uri), NULL,  &pj_str(text), NULL, NULL);
	}
	PJ_CHECK_STATUS(st, ("ERROR: SipAgent::SendInstantMessage: No se puede enviar ", "acc_id=%d dest=%s text %s", acc_id, dest_uri, text));
	return CORESIP_OK;
}

/**
* Notify application on incoming pager (i.e. MESSAGE request).
* Argument call_id will be -1 if MESSAGE request is not related to an
* existing call.
*
* See also \a on_pager2() callback for the version with \a pjsip_rx_data
* passed as one of the argument.
*
* @param call_id	    Containts the ID of the call where the IM was
*			    sent, or PJSUA_INVALID_ID if the IM was sent
*			    outside call context.
* @param from	    URI of the sender.
* @param to	    URI of the destination message.
* @param contact	    The Contact URI of the sender, if present.
* @param mime_type	    MIME type of the message.
* @param body	    The message content.
*/
void SipAgent::OnPager(pjsua_call_id call_id, const pj_str_t *from,
		     const pj_str_t *to, const pj_str_t *contact,
		     const pj_str_t *mime_type, const pj_str_t *body)
{
	PJ_LOG(5,(__FILE__, "SipAgent::OnPager: Recibido mensaje call_id %d, from %s, to %s contact %s mime_type %s body %s", 
		call_id, from->ptr, to->ptr, contact->ptr, mime_type->ptr, body->ptr)); 
	
	if (SipAgent::Cb.PagerCb)
	{
		pj_pool_t *tmppool = pjsua_pool_create(NULL, 512, 64);
		pj_assert(tmppool != NULL);

		pj_str_t tmpfrom;
		pj_strdup_with_null(tmppool, &tmpfrom, from);
		pj_str_t tmpto;
		pj_strdup_with_null(tmppool, &tmpto, to);
		pj_str_t tmpcontact;
		pj_strdup_with_null(tmppool, &tmpcontact, contact);
		pj_str_t tmpmime_type;
		pj_strdup_with_null(tmppool, &tmpmime_type, mime_type);
		pj_str_t tmpbody;
		pj_strdup_with_null(tmppool, &tmpbody, body);

		SipAgent::Cb.PagerCb(tmpfrom.ptr, tmpfrom.slen, tmpto.ptr, tmpto.slen, tmpcontact.ptr, tmpcontact.slen,
			tmpmime_type.ptr, tmpmime_type.slen, tmpbody.ptr, tmpbody.slen);

		pj_pool_release(tmppool);
	}
	
}

void SipAgent::ReadiniFile()
{
	//Lee el fichero ini y rellena la structura Coresip_Local_Config
	char curdir[256];
	char inipath[512];
	inipath[0] = '\0';

	if(GetCurrentDirectory(sizeof(curdir), curdir) > 0)
	{
		strcpy(inipath, curdir);
		strcat(inipath, "\\coresip.ini");
	}

	UINT DBSS = GetPrivateProfileInt("CORESIP", "Debug_BSS", 0, inipath);
	if (DBSS) Coresip_Local_Config._Debug_BSS = PJ_TRUE;
	else Coresip_Local_Config._Debug_BSS = PJ_FALSE;	
}

/** */
