#ifndef __CORESIP_SIPAGENT_H__
#define __CORESIP_SIPAGENT_H__

// No usar como ID el 0x80000000 porque puede dar problemas por ser valor negativo
#define CORESIP_CALL_ID				0x40000000
#define CORESIP_SNDDEV_ID			0x20000000
#define CORESIP_WAVPLAYER_ID		0x10000000
#define CORESIP_RDRXPORT_ID			0x08000000
#define CORESIP_SNDRXPORT_ID		0x04000000
#define CORESIP_ACC_ID				0x02000000
#define CORESIP_WAVRECORDER_ID		0x01000000

#define CORESIP_ID_TYPE_MASK		0xFF800000
#define CORESIP_ID_MASK				0x007FFFFF

//Q931 CODES. CAUSES.
#define Q931_NORMAL_CALL_CLEARING	(16)

//CallEnd. Origen de la desconexion
#define CALLEND_UNKNOWN 0
#define CALLEND_DEST	1
#define CALLEND_OTHER	2

#include "SoundPort.h"
#include "WavPlayer.h"
#include "WavRecorder.h"
#include "RdRxPort.h"
#include "SoundRxPort.h"
#include "RecordPort.h"
#include "FrecDesp.h"
#include "PresenceManag.h"
#include "SubsManager.h"
#include "WavPlayerToRemote.h"		/** AGL */
#include "ConfSubs.h"
#include "DlgSubs.h"
#include <map>
#include <string>

struct stCoresip_Local_Config
{
	//Estructura que contiene los datos obtenido de la configuracion local de la CORESIP obtenida de un fichero de configuracion
	pj_bool_t _Debug_BSS;						//Indica hay debig para el BSS
};

class SipAgent
{
public:
	//Direccion llamada
	static const int INCOM = 0;
	static const int OUTCOM = 1;
	
	static CORESIP_Callbacks Cb;
	static bool EnableMonitoring;
	static unsigned SndSamplingRate;
	static unsigned DefaultDelayBufPframes;
	static float RxLevel;
	static float TxLevel;

	/* AGL 20131121. Parámetros para el control del Cancelador de ECO */
	static unsigned EchoTail;
	static unsigned EchoLatency;
	/* FM */

	static bool IsRadio;
	static char StaticContact[256];
	static pjsip_sip_uri *pContacUrl;

	static FrecDesp *_FrecDesp;
	static PresenceManag *_PresenceManager;
	static SubsManager<ConfSubs> *_ConfManager;			//Objeto para administrar las subscripciones al evento de conferencia
	static SubsManager<DlgSubs> *_DlgManager;			//Objeto para administrar las subscripciones al evento de dialogo

	//Parametros necesarios para el cacelador de ECO de manos libres
	static pjmedia_echo_state *_EchoCancellerLCMic;		//Cancela eco entre altavoz y microfono cuando 
														//telefonia sale por altavoz
	static pj_bool_t _AltavozLCActivado;				//Si true, el altavoz LC esta activado reproduciendo audio
	static pj_lock_t *_ECLCMic_mutex;					//Mutex para el cancelador de eco altavoz LC-Mic

	static unsigned _TimeToDiscardRdInfo;				//Tiempo durante el cual no se envia RdInfo al Nodebox tras un PTT OFF

	static pj_bool_t _HaveRdAcc;						//Si vale true, entonces algun account del agente es de lipo radio GRS

	/*Parametros para simulacion de radio*/
	static unsigned _Radio_UA;							//Con valor distinto de 0, indica que se comporta como un agente de radio. 

	static pjmedia_port *_Tone_Port;					//Puerto de la pjmedia para reproducir un tono
	static pjsua_conf_port_id _Tone_Port_Id;

	static stCoresip_Local_Config Coresip_Local_Config;	

	static pj_bool_t PTT_local_activado;				//Estado del PTT local
	
public:
	static void Init(const CORESIP_Config * cfg);
	static void Start();
	static void Stop();

	static void SetSipPort(unsigned int port);
	static void SetRTPPort(unsigned int port);
	static int GetRTPPort(unsigned int *port);

	static void KeepAliveParams(char * kaPeriod, char * kaMultiplier);

	static void SetLogLevel(unsigned level);
	static void SetParams(const CORESIP_Params * info);

	static int CreateAccount(const char * acc, int defaultAcc, const char *proxy_ip=NULL);
	static void DestroyAccount(int id);

	static void SetTipoGRS(int id, CORESIP_CallFlags Flag);
	static pjsua_acc_id GetTipoGRS(int id, CORESIP_CallFlags * );

	static int CreateAccountAndRegisterInProxy(const char * accID, int defaultAcc, const char *proxy_ip, unsigned int expire_seg, const char *username, const char *pass, const char *displayName, int isfocus);

	static int AddSndDevice(const CORESIP_SndDeviceInfo * info);

	static int CreateWavPlayer(const char * file, bool loop);
	static void DestroyWavPlayer(int id);

	static int CreateWavRecorder(const char * file);
	static void DestroyWavRecorder(int id);

	static int CreateRdRxPort(const CORESIP_RdRxPortInfo * info, const char * localIp);
	static void DestroyRdRxPort(int id);

	static int CreateSndRxPort(const char * name);
	static void DestroySndRxPort(int id);

	static int RecConnectSndPort(bool on, int dev, RecordPort *recordport);
	static int RecConnectSndPorts(bool on, RecordPort *recordport);
	static int RecINVTel();
	static int RecINVRad();
	static int RecBYETel();
	static int RecBYERad();
	static int RecCallStart(int dir, CORESIP_Priority priority, const pj_str_t *ori_uri, const pj_str_t *dest_uri);
	static int RecCallEnd(int cause, int disc_origin);
	static int RecCallConnected(const pj_str_t *connected_uri);
	static int RecHold(bool on, bool llamante);
	static unsigned NumConfirmedCalls();
	static bool IsSlotValid(pjsua_conf_port_id slot);
	
	static void BridgeLink(int srcType, int src, int dstType, int dst, bool on);

	static void SendToRemote(int typeDev, int dev, bool on, const char * id, const char * ip, unsigned port);
	static void ReceiveFromRemote(const char * localIp, const char * mcastIp, unsigned mcastPort);

	static void SetVolume(int idType, int id, unsigned volume);
	static unsigned GetVolume(int idType, int id);

	static pj_bool_t OnRxRequest(pjsip_rx_data *rdata);
	static pj_bool_t OnRxResponse(pjsip_rx_data *rdata);

	static void RdPttEvent(bool on, const char *freqId, int dev, CORESIP_PttType PTT_type);
	static void RdSquEvent(bool on, const char *freqId, const char *resourceId, const char *bssMethod, unsigned int bssQidx);
	static int RecorderCmd(CORESIP_RecCmdType cmd, CORESIP_Error * error);

	/** AGL */
	static int CreateWavPlayer2Remote(const char *filename, const char * id, const char * ip, unsigned port);
	static void DestroyWavPlayer2Remote();
	/** */

	static char *Get_uaIpAdd() {return SipAgent::uaIpAdd;};

	static int CreatePresenceSubscription(char *dest_uri);
	static int DestroyPresenceSubscription(char *dest_uri);

	static int CreateConferenceSubscription(int acc_id, char *dest_uri, pj_bool_t by_proxy);
	static int DestroyConferenceSubscription(char *dest_uri);

	static int CreateDialogSubscription(int acc_id, char *dest_uri, pj_bool_t by_proxy);
	static int DestroyDialogSubscription(char *dest_uri);

	static int SendInstantMessage(int acc_id, char *dest_uri, char *text, pj_bool_t by_proxy);
	static void OnPager(pjsua_call_id call_id, const pj_str_t *from,
		     const pj_str_t *to, const pj_str_t *contact,
		     const pj_str_t *mime_type, const pj_str_t *body);
		
	static int EchoCancellerLCMic(bool on);

private:
	
	static pj_lock_t * _Lock;
	static unsigned _KeepAlivePeriod;
	static unsigned _KeepAliveMultiplier;
	static SoundPort * _SndPorts[CORESIP_MAX_SOUND_DEVICES];
	static WavPlayer * _WavPlayers[CORESIP_MAX_WAV_PLAYERS];
	static WavRecorder * _WavRecorders[CORESIP_MAX_WAV_RECORDERS];
	static RdRxPort * _RdRxPorts[CORESIP_MAX_RDRX_PORTS];
	static SoundRxPort * _SndRxPorts[CORESIP_MAX_SOUND_RX_PORTS];
	static RecordPort * _RecordPortTel;
	static RecordPort * _RecordPortRad;
	
	static std::map<std::string, SoundRxPort*> _SndRxIds;
	static pj_sock_t _Sock;
	static pj_activesock_t * _RemoteSock;

	static WavPlayerToRemote *_wp2r;	/** AGL */

	static int _NumInChannels;
	static int _NumOutChannels;
	static int _InChannels[10 * CORESIP_MAX_SOUND_DEVICES];
	static int _OutChannels[10 * CORESIP_MAX_SOUND_DEVICES];
	static pjmedia_aud_stream * _SndDev;
	static pjmedia_port * _ConfMasterPort;

	static char uaIpAdd[32];
	static char HostId[33];
	static unsigned int uaPort;
	
	static pjsua_transport_id SipTransportId;			//Id del SIP transport (direccion IP y puerto del protocolo SIP)

private:
	static pj_status_t OnWavPlayerEof(pjmedia_port * port, void * userData);
	static pj_bool_t OnDataReceived(pj_activesock_t * asock, void * data, pj_size_t size, const pj_sockaddr_t *src_addr, int addr_len, pj_status_t status);		
	static void ReadiniFile();

#ifdef PJ_USE_ASIO
	static pj_status_t RecCb(void * userData, pjmedia_frame * frame);
	static pj_status_t PlayCb(void * userData, pjmedia_frame * frame);
#endif
};

#endif
