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

typedef int		pj_bool_t;

#define CORESIP_OK								0
#define CORESIP_ERROR							1

#define CORESIP_MAX_USER_ID_LENGTH			100
#define CORESIP_MAX_FILE_PATH_LENGTH		256
#define CORESIP_MAX_ERROR_INFO_LENGTH		512
#define CORESIP_MAX_HOSTID_LENGTH			32
#define CORESIP_MAX_IP_LENGTH				25
#define CORESIP_MAX_URI_LENGTH				256
#define CORESIP_MAX_TAG_LENGTH				256
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
#define CORESIP_MAX_ZONA_LENGTH				128
#define CORESIP_MAX_BSS_LENGTH				32
#define CORESIP_MAX_SUPPORTED_LENGTH		512
#define CORESIP_MAX_NAME_LENGTH	         	20
#define CORESIP_MAX_CALLID_LENGTH			256
#define CORESIP_MAX_RSSI_QIDX				15			//Valor maximo de QIDX

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
	CORESIP_CALL_EC = 0x10,
	CORESIP_CALL_EXTERNA = 0x20,

	CORESIP_CALL_NINGUNO = 0x0,			//Se refiere a transceptor //UNIFETM: Falta en ULISES
	CORESIP_CALL_RD_IDLE = 0x12,		//UNIFETM: Falta en ULISES
	CORESIP_CALL_RD_TXRX = 0x14,		//UNIFETM: Falta en ULISES
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

//Comandos para el grabador
typedef enum CORESIP_RecCmdType			
{	
	CORESIP_REC_RESET=0,		//Resetea el servicio de grabacion
	CORESIP_REC_ENABLE,		//Activa la grabacion
	CORESIP_REC_DISABLE		//Desactiva la grabacion
} CORESIP_RecCmdType;

typedef enum CORESIP_FREQUENCY_TYPE 
{ Simple = 0, Dual = 1, FD = 2, ME = 3 } 
CORESIP_FREQUENCY_TYPE;         // 0. Normal, 1: 1+1, 2: FD, 3: EM

typedef enum CORESIP_CLD_CALCULATE_METHOD 
{ Relative, Absolute } 
CORESIP_CLD_CALCULATE_METHOD;


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

	CORESIP_CallFlags Flags_type;				//UNIFETM: Este campo falta en ULISES. En el ETM lo utiliza para indicar el tipo de agente radio. Se pasa a la callback de onIncommingCall
	//int SourcePort;							//UNIFETM: Este campo falta en ULISES. En el ETM no se utiliza. Se le asigna valor, pero para nada. Yo lo quitaria en ETM
	int DestinationPort;						//UNIFETM: Este campo falta en ULISES. Se asigna el valor en onIncomingCall. es un valor que se retorna en la callback. No es de entrada.
		

	/** */
    int PreferredCodec;							//UNIFETM: Este campo falta en ETM. Si no se quiere utilizar, asignar valor 0xFF en el ETM
    int PreferredBss;							//UNIFETM: Este campo falta en ETM. Asignar valor 0 en ETM
	//int ChangeSessionType;		// 0: No Change, 1 => RadioIdle.
	//int ChangeSendrecv;			// 0: No Change

	char Zona[CORESIP_MAX_ZONA_LENGTH + 1];		//UNIFETM: Este campo falta en ETM. Asignarle el valor 0 en ETM
    CORESIP_FREQUENCY_TYPE FrequencyType;		//UNIFETM: Este campo falta en ETM. Asignarle el valor Simple en ETM
    CORESIP_CLD_CALCULATE_METHOD CLDCalculateMethod;	//UNIFETM: Este campo falta en ETM. Asignarle el valor Relative en ETM
    int BssWindows;								//UNIFETM: Este campo falta en ETM. Asignarle el valor 0 en ETM
    pj_bool_t AudioSync;						//UNIFETM: Este campo falta en ETM. Asignarle el valor 0 en ETM
    pj_bool_t AudioInBssWindow;					//UNIFETM: Este campo falta en ETM. Asignarle el valor 0 en ETM
    pj_bool_t NotUnassignable;
	int cld_supervision_time;		//Tiempo de supervision CLD en segundos. Si el valor es 0 entonces no hay supervison de CLD. //UNIFETM: Este campo falta en ETM. Asignarle el valor 0 en ETM
	char bss_method[CORESIP_MAX_BSS_LENGTH + 1];	//UNIFETM: Este campo falta en ETM. Asignarle el valor 0 en ETM
	unsigned int porcentajeRSSI;				//Peso del valor de Qidx del tipo RSSI en el calculo del Qidx final. 0 indica que el calculo es interno (centralizado). 9 que el calculo es solo el RSSI.  //UNIFETM: Este campo falta en ETM. Asignarle el valor 0 en ETM

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

	//Referente al replaces. Esto no se necesita cuando el DstUri se obtiene de un REFER y ya tiene la info de replaces
	pj_bool_t RequireReplaces;								//Vale true si requiere replaces
	char CallIdToReplace[CORESIP_MAX_CALLID_LENGTH + 1];	//Call id de la llamada a reemplazar
	char ToTag[CORESIP_MAX_TAG_LENGTH + 1];					//Tag del To de la llamada a reemplazar
	char FromTag[CORESIP_MAX_TAG_LENGTH + 1];				//Tag del From de la llamada a reemplazar
	pj_bool_t EarlyOnly;									//Vale true si se requiere el parametro early-only en el replaces

} CORESIP_CallOutInfo;

//UNIFETM: en el ETM esta estructura tiene menos campos, Se utiliza solo para la callback. No habria que cambiar nada en el ETM
typedef struct CORESIP_CallInInfo
{
	char SrcId[CORESIP_MAX_USER_ID_LENGTH + 1];
	char SrcIp[CORESIP_MAX_IP_LENGTH + 1];
	unsigned SrcPort;
	char SrcSubId[CORESIP_MAX_USER_ID_LENGTH + 1];
	char SrcRs[CORESIP_MAX_RS_LENGTH + 1];
	char DstId[CORESIP_MAX_USER_ID_LENGTH + 1];
	char DstIp[CORESIP_MAX_IP_LENGTH + 1];
	char DstSubId[CORESIP_MAX_USER_ID_LENGTH + 1];
	char DisplayName[CORESIP_MAX_NAME_LENGTH + 1];
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

//UNIFETM: Esta estructura hay que modificarla en el ETM. para que sea como esta.
typedef struct CORESIP_PttInfo
{
	CORESIP_PttType PttType;
	unsigned short PttId;
	unsigned PttMute;
	unsigned ClimaxCld;
	unsigned Squ;				//Si valor es 1 -> squelch on, 0 -> off
	unsigned RssiQidx;			//Valor de Qidx del tipo RSSI que se envia cuando es un agente simulador de radio y Squ es activo

} CORESIP_PttInfo;

//UNIFETM: Esta estructura cambia en el ETM. Pero las que utiliza el ETM siguen estando. No hay que cambiar nada en el ETM
typedef struct CORESIP_RdInfo
{
	CORESIP_PttType PttType;
	unsigned short PttId;
	int PttMute;
	int Squelch;
	int Sct;

	int rx_rtp_port;
	int rx_qidx;
	pj_bool_t rx_selected;
	int tx_rtp_port;
	int tx_cld;
	int tx_owd;

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
	void (*ConfInfoCb)(int call, CORESIP_ConfInfo * confInfo, const char *from_uri, int from_uri_len);
	void (*OptionsReceiveCb)(const char * fromUri, const char * callid, const int statusCode, const char *supported, const char *allow );
																									//UNIFETM: En el ETM esta funcion tiene menos parametros
	//void (*OptionsReceiveCb)(const char * fromUri);												//Esta es la del ETM. 


	void (*WG67NotifyCb)(void * wg67Subscription, CORESIP_WG67Info * info, void * userData);
	void (*InfoReceivedCb)(int call, const char * info, unsigned int lenInfo);	
	void (*IncomingSubscribeConfCb)(int call, const char *from_uri, const int from_uri_len);		//UNIFETM: Este campo falta en ETM. Inicializarlo a NULL
	void (*Presence_callback)(char *dst_uri, int subscription_status, int presence_status);			//UNIFETM: Este campo falta en ETM. Inicializarlo a NULL
	void (*FinWavCb)(int Code);																		//UNIFETM: Este campo falta en ULISES. Inicializarlo a NULL

	void (*DialogNotifyCb)(const char *xml_body, unsigned int length);			//Callback que se llama cuando se recibe un notify al evento de dialogo
	
	void (*PagerCb)(const char *from_uri, const int from_uri_len,
		     const char *to_uri, const int to_uri_len, const char *contact_uri, const int contact_uri_len,
		     const char *mime_type, const int mime_type_len, const char *body, const int body_len);
																				//Callback que se llama cuando se recibe un mensaje de texto

	 
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
	char HostId[CORESIP_MAX_HOSTID_LENGTH + 1];			//UNIFETM: Este campo falta en ETM. Inicializarlo a 0
	char IpAddress[CORESIP_MAX_IP_LENGTH + 1];
	unsigned Port;
	unsigned RtpPorts;									//Valor por el que empiezan a crearse los puertos RTP	//UNIFETM: Este campo falta en ETM. Inicializarlo a 0

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
	unsigned EchoTail;									//UNIFETM: Este campo falta en ETM. Inicializarlo a 100
	unsigned EchoLatency;								//UNIFETM: Este campo falta en ETM. inicializarlo a 0
/* FM */

	// Grabación según norma ED-137
    unsigned RecordingEd137;							//UNIFETM: Este campo falta en ETM. inicializarlo a 0

	unsigned max_calls;		//Máximo número de llamadas que soporta el agente.  //UNIFETM: Este campo falta en ETM. inicializarlo a numero de llamadas maximas. Ej. 32

	unsigned Radio_UA;		//Con valor distinto de 0, indica que se comporta como un agente de radio. //UNIFETM: Este campo falta en ETM. inicializarlo a 0

	unsigned TimeToDiscardRdInfo;		//Tiempo durante el cual no se envia RdInfo al Nodebox tras un PTT OFF. //UNIFETM: Este campo falta en ETM. inicializarlo a 0

} CORESIP_Config;

typedef struct CORESIP_Impairments
{
	int Perdidos;
	int Duplicados;
	int LatMin;
	int LatMax;
} CORESIP_Impairments;

/*Callback para recibir notificaciones por la subscripcion de presencia*/
/*	dst_uri: uri del destino cuyo estado de presencia ha cambiado.
 *	subscription_status: vale 0 la subscripcion al evento no ha tenido exito. 
 *	presence_status: vale 0 si no esta presente. 1 si esta presente.
 */
typedef void (*SubPresCb)(char *dst_uri, int subscription_status, int presence_status);

#ifdef __cplusplus
extern "C" {
#endif

	CORESIP_API int	CORESIP_Init(const CORESIP_Config * info, CORESIP_Error * error);
	CORESIP_API int	CORESIP_Start(CORESIP_Error * error);
	CORESIP_API void CORESIP_End();

	CORESIP_API int	CORESIP_SetSipPort(int port, CORESIP_Error * error);

	CORESIP_API int	CORESIP_SetLogLevel(unsigned level, CORESIP_Error * error);
	CORESIP_API int	CORESIP_SetParams(const CORESIP_Params * info, CORESIP_Error * error);

	CORESIP_API int	CORESIP_CreateAccount(const char * acc, int defaultAcc, int * accId, CORESIP_Error * error);
	CORESIP_API int CORESIP_CreateAccountProxyRouting(const char * acc, int defaultAcc, int * accId, const char *proxy_ip, CORESIP_Error * error);
	CORESIP_API int CORESIP_CreateAccountAndRegisterInProxy(const char * acc, int defaultAcc, int * accId, const char *proxy_ip, 
														unsigned int expire_seg, const char *username, const char *pass, const char * displayName, int isfocus, CORESIP_Error * error);
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
	CORESIP_API int	CORESIP_CallTransfer(int call, int dstCall, const char * dst, const char *display_name, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallPtt(int call, const CORESIP_PttInfo * info, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallConference(int call, int conf, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallSendConfInfo(int call, const CORESIP_ConfInfo * info, CORESIP_Error * error);
	CORESIP_API int CORESIP_SendConfInfoFromAcc(int accId, const CORESIP_ConfInfo * info, CORESIP_Error * error);
	CORESIP_API int	CORESIP_CallSendInfo(int call, const char * info, CORESIP_Error * error);

	CORESIP_API int	CORESIP_TransferAnswer(const char * tsxKey, void * txData, void * evSub, unsigned code, CORESIP_Error * error);
	CORESIP_API int	CORESIP_TransferNotify(void * evSub, unsigned code, CORESIP_Error * error);

	CORESIP_API int	CORESIP_SendOptionsMsg(const char * dst, char * callid, int isRadio, CORESIP_Error * error);
	CORESIP_API int CORESIP_SendOptionsMsgProxy(const char * dst, char * callid, int isRadio, CORESIP_Error * error);

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
	CORESIP_API int CORESIP_RdPttEvent(bool on, const char *freqId, int dev, CORESIP_Error * error, CORESIP_PttType PTT_type = CORESIP_PTT_NORMAL);
	//CORESIP_API int CORESIP_RdSquEvent(bool on, const char *freqId, int dev, CORESIP_Error * error);
	CORESIP_API int CORESIP_RdSquEvent(bool on, const char *freqId, const char *resourceId, const char *bssMethod, unsigned int bssQidx, CORESIP_Error * error);
	CORESIP_API int CORESIP_RecorderCmd(CORESIP_RecCmdType cmd, CORESIP_Error * error);

	CORESIP_API int CORESIP_CreatePresenceSubscription(char *dest_uri, CORESIP_Error * error);
	CORESIP_API int CORESIP_DestroyPresenceSubscription(char *dest_uri, CORESIP_Error * error);

	CORESIP_API int CORESIP_CreateConferenceSubscription(int accId, char *dest_uri, pj_bool_t by_proxy, CORESIP_Error * error);
	CORESIP_API int CORESIP_DestroyConferenceSubscription(char *dest_uri, CORESIP_Error * error);

	CORESIP_API int CORESIP_CreateDialogSubscription(int accId, char *dest_uri, pj_bool_t by_proxy, CORESIP_Error * error);
	CORESIP_API int CORESIP_DestroyDialogSubscription(char *dest_uri, CORESIP_Error * error);

	CORESIP_API int CORESIP_SendInstantMessage(int acc_id, char *dest_uri, char *text, pj_bool_t by_proxy, CORESIP_Error * error);

	CORESIP_API int CORESIP_EchoCancellerLCMic(bool on, CORESIP_Error * error);

	CORESIP_API int CORESIP_SetTipoGRS(int accId, CORESIP_CallFlags Flag, CORESIP_Error * error);
	CORESIP_API int CORESIP_SetImpairments(int call, CORESIP_Impairments * impairments, CORESIP_Error * error);

#ifdef __cplusplus
}
#endif

#endif