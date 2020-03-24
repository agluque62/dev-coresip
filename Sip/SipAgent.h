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
#include "WavPlayerToRemote.h"		/** AGL */
#include <map>
#include <string>

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

public:
	static void Init(const CORESIP_Config * cfg);
	static void Start();
	static void Stop();

	static void KeepAliveParams(char * kaPeriod, char * kaMultiplier);

	static void SetLogLevel(unsigned level);
	static void SetParams(const CORESIP_Params * info);

	static int CreateAccount(const char * acc, int defaultAcc);
	static void DestroyAccount(int id);

	static int CreateAccountAndRegisterInProxy(const char * accID, int defaultAcc, const char *proxy_ip, unsigned int expire_seg, const char *username, const char *pass);

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

	static pj_bool_t OnRxResponse(pjsip_rx_data *rdata);

	static void RdPttEvent(bool on, const char *freqId, int dev);
	static void RdSquEvent(bool on, const char *freqId, int dev);

	/** AGL */
	static int CreateWavPlayer2Remote(const char *filename, const char * id, const char * ip, unsigned port);
	static void DestroyWavPlayer2Remote();
	/** */

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
	static unsigned int uaPort;
private:
	static pj_status_t OnWavPlayerEof(pjmedia_port * port, void * userData);
	static pj_bool_t OnDataReceived(pj_activesock_t * asock, void * data, pj_size_t size, const pj_sockaddr_t *src_addr, int addr_len, pj_status_t status);		

#ifdef PJ_USE_ASIO
	static pj_status_t RecCb(void * userData, pjmedia_frame * frame);
	static pj_status_t PlayCb(void * userData, pjmedia_frame * frame);
#endif
};

#endif
