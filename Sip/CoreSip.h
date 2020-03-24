/**
 * @file CoreSip.h
 * @brief Define la interfaz pública de la librería CORESIP.dll
 *
 *	Contiene las definiciones y prototipos de acceso a la libreria 'coresip'
 *	para interfasarse a una tajeta de sonido.
 *
 *	@addtogroup CORESIP
 */
#ifndef __CORESIP_H__
#define __CORESIP_H__

#if defined(_WIN32) && defined(_USRDLL)
#	ifdef CORESIP_EXPORTS
#		define CORESIP_API __declspec(dllexport)
#	else
#		define CORESIP_API __declspec(dllimport)
#	endif
#else
#	define CORESIP_API
#endif

#define CORESIP_OK								0
#define CORESIP_ERROR							1

#define CORESIP_MAX_USER_ID_LENGTH			100
#define CORESIP_MAX_FILE_PATH_LENGTH		256
#define CORESIP_MAX_ERROR_INFO_LENGTH		512
#define CORESIP_MAX_HOSTID_LENGTH			32
#define CORESIP_MAX_IP_LENGTH				25
#define CORESIP_MAX_URI_LENGTH				256
#define CORESIP_MAX_SOUND_DEVICES			20
#define CORESIP_MAX_WAV_PLAYERS				50
#define CORESIP_MAX_WAV_RECORDERS			50
#define CORESIP_MAX_RDRX_PORTS				50
#define CORESIP_MAX_SOUND_RX_PORTS			50
#define CORESIP_MAX_RS_LENGTH				128
#define CORESIP_MAX_REASON_LENGTH			128
#define CORESIP_MAX_WG67_SUBSCRIBERS		25
#define CORESIP_MAX_CODEC_LENGTH			50
#define CORESIP_MAX_CONF_USERS				25
#define CORESIP_MAX_CONF_STATE_LENGTH		25
#define CORESIP_MAX_SLOTSTOSNDPORTS			50	

#ifdef _ED137_
// PlugTest FAA 05/2011
// Recording
#define CORESIP_MAX_FRECUENCY_LENGTH		CORESIP_MAX_RS_LENGTH + 1
#define CORESIP_TIME_LENGTH					28
#define CORESIP_CALLREF_LENGTH				50
#define CORESIP_CONNREF_LENGTH				50
// OVR Calls
#define CORESIP_MAX_OVR_CALLS_MEMBERS		10
#endif

typedef enum CORESIP_CallType
{
	CORESIP_CALL_IA,
	CORESIP_CALL_MONITORING,
	CORESIP_CALL_GG_MONITORING,
	CORESIP_CALL_AG_MONITORING,
	CORESIP_CALL_DIA,
	CORESIP_CALL_RD,
#ifdef _ED137_
	CORESIP_CALL_RRC,
	CORESIP_CALL_OVR,
	CORESIP_CALL_RECORDING,
	CORESIP_CALL_RXTXRD,
#endif
	CORESIP_CALL_UNKNOWN

} CORESIP_CallType;


typedef enum CORESIP_Priority
{
	CORESIP_PR_EMERGENCY,
	CORESIP_PR_URGENT,
	CORESIP_PR_NORMAL,
	CORESIP_PR_NONURGENT,
	CORESIP_PR_UNKNOWN

} CORESIP_Priority;


typedef enum CORESIP_CallFlags
{
	CORESIP_CALL_CONF_FOCUS = 0x1,
	CORESIP_CALL_RD_COUPLING = 0x2,
	CORESIP_CALL_RD_RXONLY = 0x4,
	CORESIP_CALL_RD_TXONLY = 0x8,
	CORESIP_CALL_EC = 0x10

} CORESIP_CallFlags;


typedef enum CORESIP_CallState
{
	CORESIP_CALL_STATE_NULL,					/**< Before INVITE is sent or received  */
	CORESIP_CALL_STATE_CALLING,				/**< After INVITE is sent		    */
	CORESIP_CALL_STATE_INCOMING,				/**< After INVITE is received.	    */
	CORESIP_CALL_STATE_EARLY,					/**< After response with To tag.	    */
	CORESIP_CALL_STATE_CONNECTING,			/**< After 2xx is sent/received.	    */
	CORESIP_CALL_STATE_CONFIRMED,			/**< After ACK is sent/received.	    */
	CORESIP_CALL_STATE_DISCONNECTED,		/**< Session is terminated.		    */

} CORESIP_CallState;


typedef enum CORESIP_CallRole
{
	CORESIP_CALL_ROLE_UAC,
	CORESIP_CALL_ROLE_UCS

} CORESIP_CallRole;


typedef enum CORESIP_MediaStatus
{
	CORESIP_MEDIA_NONE,
	CORESIP_MEDIA_ACTIVE,
	CORESIP_MEDIA_LOCAL_HOLD,
	CORESIP_MEDIA_REMOTE_HOLD,
	CORESIP_MEDIA_ERROR

} CORESIP_MediaStatus;


typedef enum CORESIP_MediaDir
{
	CORESIP_DIR_NONE,
	CORESIP_DIR_SENDONLY,
	CORESIP_DIR_RECVONLY,
	CORESIP_DIR_SENDRECV

} CORESIP_MediaDir;


typedef enum CORESIP_PttType
{
	CORESIP_PTT_OFF,
	CORESIP_PTT_NORMAL,
	CORESIP_PTT_COUPLING,
	CORESIP_PTT_PRIORITY,
	CORESIP_PTT_EMERGENCY

} CORESIP_PttType;

typedef enum CORESIP_SndDevType
{
	CORESIP_SND_INSTRUCTOR_MHP,
	CORESIP_SND_ALUMN_MHP,
	CORESIP_SND_MAX_IN_DEVICES,
	CORESIP_SND_MAIN_SPEAKERS = CORESIP_SND_MAX_IN_DEVICES,
	CORESIP_SND_LC_SPEAKER,
	CORESIP_SND_RD_SPEAKER,
	CORESIP_SND_UNKNOWN

} CORESIP_SndDevType;


typedef struct CORESIP_Error
{
	int Code;
	char File[CORESIP_MAX_FILE_PATH_LENGTH + 1];
	char Info[CORESIP_MAX_ERROR_INFO_LENGTH + 1];

} CORESIP_Error;


typedef struct CORESIP_SndDeviceInfo
{
	CORESIP_SndDevType Type;

	int OsInDeviceIndex;
	int OsOutDeviceIndex;

} CORESIP_SndDeviceInfo;


typedef struct CORESIP_RdRxPortInfo
{
	unsigned ClkRate;
	unsigned ChannelCount;
	unsigned BitsPerSample;
	unsigned FrameTime;
	char Ip[CORESIP_MAX_IP_LENGTH + 1];
	unsigned Port;

} CORESIP_RdRxPortInfo;


typedef struct CORESIP_CallInfo
{
	int AccountId;
	CORESIP_CallType Type;
	CORESIP_Priority Priority;
	CORESIP_CallFlags Flags;
#ifdef _ED137_
	CORESIP_MediaDir Dir;
	int Codecs;
	int BssMethods;
	char Frequency[CORESIP_MAX_FRECUENCY_LENGTH + 1];
#endif
} CORESIP_CallInfo;


typedef struct CORESIP_CallOutInfo
{
	char DstUri[CORESIP_MAX_URI_LENGTH + 1];

	char ReferBy[CORESIP_MAX_URI_LENGTH + 1];

	char RdFr[CORESIP_MAX_RS_LENGTH + 1];					// Frecuencia
	char RdMcastAddr[CORESIP_MAX_IP_LENGTH + 1];
	unsigned RdMcastPort;

} CORESIP_CallOutInfo;


typedef struct CORESIP_CallInInfo
{
	char SrcId[CORESIP_MAX_USER_ID_LENGTH + 1];
	char SrcIp[CORESIP_MAX_IP_LENGTH + 1];
	char SrcSubId[CORESIP_MAX_USER_ID_LENGTH + 1];
	char SrcRs[CORESIP_MAX_RS_LENGTH + 1];
	char DstId[CORESIP_MAX_USER_ID_LENGTH + 1];
	char DstIp[CORESIP_MAX_IP_LENGTH + 1];
	char DstSubId[CORESIP_MAX_USER_ID_LENGTH + 1];

} CORESIP_CallInInfo;


typedef struct CORESIP_CallTransferInfo
{
	void * TxData;
	void * EvSub;
	char TsxKey[CORESIP_MAX_RS_LENGTH + 1];

	char ReferBy[CORESIP_MAX_URI_LENGTH + 1];
	char ReferTo[2 * CORESIP_MAX_URI_LENGTH + 1];

	char DstId[CORESIP_MAX_USER_ID_LENGTH + 1];
	char DstIp[CORESIP_MAX_IP_LENGTH + 1];
	char DstSubId[CORESIP_MAX_USER_ID_LENGTH + 1];
	char DstRs[CORESIP_MAX_RS_LENGTH + 1];

} CORESIP_CallTransferInfo;


typedef struct CORESIP_CallStateInfo
{	
	CORESIP_CallState State;
	CORESIP_CallRole Role;

	int LastCode;										// Util cuando State == PJSIP_INV_STATE_DISCONNECTED
	char LastReason[CORESIP_MAX_REASON_LENGTH + 1];

	int LocalFocus;
	int RemoteFocus;
	CORESIP_MediaStatus MediaStatus;
	CORESIP_MediaDir MediaDir;

	// CORESIP_CALL_RD y PJSIP_INV_STATE_CONFIRMED
	unsigned short PttId;
	unsigned ClkRate;
	unsigned ChannelCount;
	unsigned BitsPerSample;
	unsigned FrameTime;

} CORESIP_CallStateInfo;


typedef struct CORESIP_PttInfo
{
	CORESIP_PttType PttType;
	unsigned short PttId;
	unsigned ClimaxCld;

} CORESIP_PttInfo;


typedef struct CORESIP_RdInfo
{
	CORESIP_PttType PttType;
	unsigned short PttId;
	int Squelch;
	int Sct;
	int Bss;
	int BssMethod;
	int BssValue;

} CORESIP_RdInfo;

#ifdef _ED137_
// PlugTest FAA 05/2011
typedef enum CORESIP_TypeCrdInfo
{
	CORESIP_CRD_SET_PARAMETER,
	CORESIP_CRD_RECORD,
	CORESIP_CRD_PAUSE,
	CORESIP_CRD_PTT,
	CORESIP_SQ	
} CORESIP_TypeCrdInfo;

typedef struct CORESIP_CRD
{
	CORESIP_TypeCrdInfo _Info;
	char CallRef[CORESIP_CALLREF_LENGTH+1];
    char ConnRef[CORESIP_CONNREF_LENGTH + 1];
    int Direction;
    int Priority;
	char CallingNr[CORESIP_MAX_URI_LENGTH + 1];
    char CalledNr[CORESIP_MAX_URI_LENGTH + 1];
    char SetupTime[CORESIP_TIME_LENGTH + 1];

    char ConnectedNr[CORESIP_MAX_URI_LENGTH + 1];
	char ConnectedTime[CORESIP_TIME_LENGTH + 1];

     char DisconnectTime[CORESIP_TIME_LENGTH + 1];
     int DisconnectCause;
	int DisconnectSource;

    char FrecuencyId[CORESIP_MAX_FRECUENCY_LENGTH + 1];

	char Squ[CORESIP_TIME_LENGTH + 1];
	char Ptt[CORESIP_TIME_LENGTH + 1];

} CORESIP_CRD;
#endif

typedef struct CORESIP_WG67Info
{
	char DstUri[CORESIP_MAX_URI_LENGTH + 1];
	char LastReason[CORESIP_MAX_REASON_LENGTH + 1];

	int SubscriptionTerminated;
	unsigned SubscribersCount;

	struct
	{
		unsigned short PttId;
		char SubsUri[CORESIP_MAX_URI_LENGTH + 1];

	} Info[CORESIP_MAX_WG67_SUBSCRIBERS];

} CORESIP_WG67Info;

#ifdef _ED137_
// PlugTest FAA 05/2011
typedef struct CORESIP_EstablishedOvrCallMembers
{
	unsigned short MembersCount;
	struct
	{
	char Member[CORESIP_MAX_URI_LENGTH + 1];
	int CallId;
	bool IncommingCall;
	}
	EstablishedOvrCallMembers[CORESIP_MAX_OVR_CALLS_MEMBERS];

} CORESIP_EstablishedOvrCallMembers;
#endif

typedef struct CORESIP_ConfInfo
{
	unsigned Version;
	unsigned UsersCount;
	char State[CORESIP_MAX_CONF_STATE_LENGTH + 1];

	struct
	{
		char Id[CORESIP_MAX_URI_LENGTH + 1];
		char Name[CORESIP_MAX_USER_ID_LENGTH + 1];
		char Role[CORESIP_MAX_USER_ID_LENGTH + 1];
		char State[CORESIP_MAX_CONF_STATE_LENGTH + 1];

	} Users[CORESIP_MAX_CONF_USERS];

} CORESIP_ConfInfo;


typedef struct CORESIP_Callbacks
{
	void * UserData;

	void (*LogCb)(int level, const char * data, int len);
	void (*KaTimeoutCb)(int call);
	void (*RdInfoCb)(int call, CORESIP_RdInfo * info);
	void (*CallStateCb)(int call, CORESIP_CallInfo * info, CORESIP_CallStateInfo * stateInfo);
	void (*CallIncomingCb)(int call, int call2replace, CORESIP_CallInfo * info, CORESIP_CallInInfo * inInfo);
	void (*TransferRequestCb)(int call, CORESIP_CallInfo * info, CORESIP_CallTransferInfo * transferInfo);
	void (*TransferStatusCb)(int call, int code);
	void (*ConfInfoCb)(int call, CORESIP_ConfInfo * confInfo);
	void (*OptionsReceiveCb)(const char * fromUri);
	void (*WG67NotifyCb)(void * wg67Subscription, CORESIP_WG67Info * info, void * userData);
	void (*InfoReceivedCb)(int call, const char * info, unsigned int lenInfo);
#ifdef _ED137_
	// PlugTest FAA 05/2011
	void (*OnUpdateOvrCallMembers)(CORESIP_EstablishedOvrCallMembers info);
	void (*InfoCrd)(CORESIP_CRD InfoCrd);
#endif
} CORESIP_Callbacks;


typedef struct CORESIP_Params
{
	int EnableMonitoring;

	unsigned KeepAlivePeriod;
	unsigned KeepAliveMultiplier;

} CORESIP_Params;


typedef struct CORESIP_Config
{
	char HostId[CORESIP_MAX_HOSTID_LENGTH + 1];
	char IpAddress[CORESIP_MAX_IP_LENGTH + 1];
	unsigned Port;

	CORESIP_Callbacks Cb;
	char DefaultCodec[CORESIP_MAX_CODEC_LENGTH + 1];
	unsigned DefaultDelayBufPframes;
	unsigned DefaultJBufPframes;
	unsigned SndSamplingRate;
	float RxLevel;
	float TxLevel;
	unsigned LogLevel;

	unsigned TsxTout;
	unsigned InvProceedingIaTout;
	unsigned InvProceedingMonitoringTout;
	unsigned InvProceedingDiaTout;
	unsigned InvProceedingRdTout;
/* AGL 20131121. Variables para la configuracion del Cancelador de Eco */
	unsigned EchoTail;
	unsigned EchoLatency;
/* FM */

	// Grabación según norma ED-137
    unsigned RecordingEd137;

} CORESIP_Config;


#ifdef __cplusplus
extern "C" {
#endif

	CORESIP_API int	CORESIP_Init(const CORESIP_Config * info, CORESIP_Error * error);
	CORESIP_API int	CORESIP_Start(CORESIP_Error * error);
	CORESIP_API void CORESIP_End();

	CORESIP_API int	CORESIP_SetLogLevel(unsigned level, CORESIP_Error * error);
	CORESIP_API int	CORESIP_SetParams(const CORESIP_Params * info, CORESIP_Error * error);

	CORESIP_API int	CORESIP_CreateAccount(const char * acc, int defaultAcc, int * accId, CORESIP_Error * error);
	CORESIP_API int CORESIP_CreateAccountAndRegisterInProxy(const char * acc, int defaultAcc, int * accId, const char *proxy_ip, 
														unsigned int expire_seg, const char *username, const char *pass, CORESIP_Error * error);
	CORESIP_API int	CORESIP_DestroyAccount(int accId, CORESIP_Error * error);

	CORESIP_API int	CORESIP_AddSndDevice(const CORESIP_SndDeviceInfo * info, int * dev, CORESIP_Error * error);

	CORESIP_API int	CORESIP_CreateWavPlayer(const char * file, unsigned loop, int * wavPlayer, CORESIP_Error * error);
	CORESIP_API int	CORESIP_DestroyWavPlayer(int wavPlayer, CORESIP_Error * error);
	
	CORESIP_API int	CORESIP_CreateWavRecorder(const char * file, int * wavRecorder, CORESIP_Error * error);
	CORESIP_API int	CORESIP_DestroyWavRecorder(int wavRecorder, CORESIP_Error * error);

	CORESIP_API int	CORESIP_CreateRdRxPort(const CORESIP_RdRxPortInfo * info, const char * localIp, int * rdRxPort, CORESIP_Error * error);
	CORESIP_API int	CORESIP_DestroyRdRxPort(int rdRxPort, CORESIP_Error * error);

	CORESIP_API int	CORESIP_CreateSndRxPort(const char * id, int * sndRxPort, CORESIP_Error * error);
	CORESIP_API int	CORESIP_DestroySndRxPort(int sndRxPort, CORESIP_Error * error);

	CORESIP_API int	CORESIP_BridgeLink(int src, int dst, int on, CORESIP_Error * error);

	CORESIP_API int	CORESIP_SendToRemote(int dev, int on, const char * id, const char * ip, unsigned port, CORESIP_Error * error);
	CORESIP_API int	CORESIP_ReceiveFromRemote(const char * localIp, const char * mcastIp, unsigned mcastPort, CORESIP_Error * error);

	CORESIP_API int	CORESIP_SetVolume(int id, unsigned volume, CORESIP_Error * error);
	CORESIP_API int	CORESIP_GetVolume(int id, unsigned * volume, CORESIP_Error * error);

	CORESIP_API int	CORESIP_CallMake(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo, int * call, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallHangup(int call, unsigned code, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallAnswer(int call, unsigned code, int addToConference, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallHold(int call, int hold, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallTransfer(int call, int dstCall, const char * dst, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallPtt(int call, const CORESIP_PttInfo * info, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallConference(int call, int conf, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallSendConfInfo(int call, const CORESIP_ConfInfo * info, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallSendInfo(int call, const char * info, CORESIP_Error * error);

	CORESIP_API int	CORESIP_TransferAnswer(const char * tsxKey, void * txData, void * evSub, unsigned code, CORESIP_Error * error);
	CORESIP_API int	CORESIP_TransferNotify(void * evSub, unsigned code, CORESIP_Error * error);

	CORESIP_API int	CORESIP_SendOptionsMsg(const char * dst, CORESIP_Error * error);

	//CORESIP_API int	CORESIP_CallAddToBss(int call, void * bssGroup, CORESIP_Error * error);
	//CORESIP_API int	CORESIP_CallRemoveFromBss(int call, void * bssGroup, CORESIP_Error * error);

	//CORESIP_API int	CORESIP_CreateBssGroup(unsigned outSoundDev, void ** bssGroup, CORESIP_Error * error);
	//CORESIP_API int	CORESIP_DestroyBssGroup(void * bssGroup, CORESIP_Error * error);

	CORESIP_API int	CORESIP_CreateWG67Subscription(const char * dst, void ** wg67Subscription, CORESIP_Error * error);
	CORESIP_API int	CORESIP_DestroyWG67Subscription(void * wg67Subscription, CORESIP_Error * error);

	//CORESIP_API int	CORESIP_ConferenceMute(unsigned conference, unsigned mute, CORESIP_Error * error);

	/** AGL. Para el SELCAL */
	CORESIP_API int CORESIP_Wav2RemoteStart(const char *filename, const char * id, const char * ip, unsigned port, void (*eofCb)(void *), CORESIP_Error * error);
	CORESIP_API int CORESIP_Wav2RemoteEnd(void *, CORESIP_Error * error);
	/****/
	CORESIP_API int CORESIP_RdPttEvent(bool on, const char *freqId, int dev, CORESIP_Error * error);
	CORESIP_API int CORESIP_RdSquEvent(bool on, const char *freqId, int dev, CORESIP_Error * error);

#ifdef __cplusplus
}
#endif

#endif