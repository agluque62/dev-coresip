#ifndef __CORESIP_CALL_H__
#define __CORESIP_CALL_H__

#include <atomic>
#include "processor.h"
#include "IIR_FILT.h"

enum bss_method_types
{
	NINGUNO,
	RSSI,
	RSSI_NUC,
	CENTRAL
};

class SipCall
{
public:
	SipCall(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo);
	SipCall(pjsua_call_id call_id, const CORESIP_CallInfo * info);
	~SipCall();


	char DstUri[CORESIP_MAX_URI_LENGTH + 1];
	CORESIP_CallInfo _Info;
	int _Index_group;						//Indice que identifica el grupo al que pertenece esta llamada
	int _Index_sess;						//Indice que identifica la sesion dentro del grupo
	pj_timer_entry window_timer;			//Timer para la ventana de decisi�n del BSS
	pj_bool_t _Sending_Multicast_enabled;	//Indica si puede enviar audio por multicast a los puestos
	CORESIP_RdInfo RdInfo_prev;
	pj_mutex_t *RdInfo_prev_mutex;

	pj_timer_entry Ptt_off_timer;			//Timer que arranca cuando en un receptor o transceptor hay Ptt off. 
											//Durante ese tiempo no se envia rdinfo a la aplicacion por la callback
	static void Ptt_off_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te);
											//Callback del timer

	int squ_status;							//Estado del squelch

	int GetSyncBss();
	CORESIP_CallInfo *GetCORESIP_CallInfo();
	int Hacer_la_llamada_saliente();
	
	static int New(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo);

	static void Hangup(pjsua_call_id call_id, unsigned code);
	static int Force_Hangup(pjsua_call_id call_id, unsigned code);
	static void Answer(pjsua_call_id call_id, unsigned code, bool addToConference);
	static void Hold(pjsua_call_id call_id, bool hold);
	static void Transfer(pjsua_call_id call_id, pjsua_call_id dst_call_id, const char * dst, const char *display_name);
	static void Ptt(pjsua_call_id call_id, const CORESIP_PttInfo * info);
	static void Conference(pjsua_call_id call_id, bool conf);
	static void SendConfInfo(pjsua_call_id call_id, const CORESIP_ConfInfo * info);
	static void SendInfoMsg(pjsua_call_id call_id, const char * info);

	static void TransferAnswer(const char * tsxKey, void * txData, void * evSub, unsigned code);
	static void TransferNotify(void * evSub, unsigned code);
	static void SendOptionsMsg(const char * target, char *callid, int isRadio, pj_bool_t by_proxy=PJ_FALSE);

	static void OnRdRtp(void * stream, void * frame, void * codec, unsigned seq, pj_uint32_t rtp_ext_info);
	static void OnRdInfoChanged(void * stream, void *ext_type_length, pj_uint32_t rtp_ext_info, const void * p_rtp_ext_info, pj_uint32_t rtp_ext_length);
	static void OnKaTimeout(void * stream);
	static void OnCreateSdp(pj_pool_t *pool, int call_id, void * local_sdp, void * incoming_rdata);
#ifdef _ED137_
	static pj_status_t OnNegociateSdp(void * inv, void * neg, void * rdata);
	// PlugTest FAA 05/2011
	// OVR calls
	static void UpdateEstablishedCallMembers(pjsua_call_id call_id, const char * info, bool adding);
	static void AddOvrMember(SipCall * call, pjsua_call_id call_id, const char * info, bool incommingCall);
	static void UpdateInfoSid(SipCall * call, pjmedia_sdp_neg * neg);
	static void RemoveOvrMember(pjsua_call_id call_id, const char * info);
	static bool FindSdpAttribute(pjsua_call_id call_id, char * attr, char * value);
	static void InfoMsgReceived(pjsua_call_id call_id,void * data,unsigned int len);
	static void AudioLoopClosure(SipCall * call, char * srcUri);
	static bool OVRCallMemberIn(char  * szBuffer);
	static char * ParseUri(char * szBuffer, char * info);
	static void MixeMonitoring(SipCall * call, int call_id, int port);
#endif


	static void OnStateChanged(pjsua_call_id call_id, pjsip_event * e);
	static void OnIncommingCall(pjsua_acc_id acc_id, pjsua_call_id call_id, pjsua_call_id replace_call_id, pjsip_rx_data * rdata);
	static void OnMediaStateChanged(pjsua_call_id call_id);
	static void OnTransferRequest(pjsua_call_id call_id, 
		const pj_str_t * refer_to, const pj_str_t * refer_by, 
		const pj_str_t * tsxKey, pjsip_tx_data * tdata, pjsip_evsub * sub);
	static void OnTransferStatusChanged(pjsua_call_id based_call_id, int st_code, const pj_str_t *st_text, pj_bool_t final, pj_bool_t *p_cont);
	static void OnReplaceRequest(pjsua_call_id replace_call_id, pjsip_rx_data * rdata, int * code, pj_str_t * st_text);
	static void OnReplaced(pjsua_call_id replace_call_id, pjsua_call_id new_call_id);
	static void OnTsxStateChanged(pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e);
	static void OnStreamCreated(pjsua_call_id call_id, pjmedia_session * sess, unsigned stream_idx, pjmedia_port **p_port);
	static void OnStreamDestroyed(pjsua_call_id call_id, pjmedia_session *sess, unsigned stream_idx);
	static pj_bool_t OnTxRequest(pjsip_tx_data *txdata);

	//ETM
	static void SetImpairments(pjsua_call_id call_id, CORESIP_Impairments *impcfg);

	/** ED137B */
	static pj_str_t gWG67VersionName;
	static pj_str_t gWG67VersionRadioValue;
	static pj_str_t gWG67VersionTelefValue;
	static void Wg67VersionSet(pjsip_tx_data *txdata, pj_str_t *valor);
		

private:
	static const int SQU_OFF = 0;
	static const int SQU_ON = 1;
	static const int WAIT_INIT_TIME_seg = 4;
	static const int WAIT_INIT_TIME_ms = 500;

	static const unsigned AUDIO_PACKET = 0;
	static const unsigned RESET_PACKET = 1;

	static const int MAX_BSS_SQU = 200;		//maxima cantidad de valores BSS almacenados en bss_rx desde que se activa un squelch

	static const unsigned int MIN_porcentajeRSSI = 0;	//Minimo valor del Peso del valor de Qidx del tipo RSSI en el calculo del Qidx final.
	static const unsigned int MAX_porcentajeRSSI = 10;	//Maximo valor del Peso del valor de Qidx del tipo RSSI en el calculo del Qidx final.
	static const pj_uint32_t MAX_RSSI_ESCALE = 15;
	static const pj_uint32_t MAX_CENTRAL_ESCALE = 50;
	static const pj_uint32_t MAX_QIDX_ESCALE = 31;
	static const pj_uint8_t RSSI_METHOD = 0;
	static const pj_uint8_t NUC_METHOD = 0x4;

	static std::atomic_int SipCallCount;
	
	struct {
		pjsip_generic_string_hdr subject, priority, referBy, require, replaces;
		pjsua_acc_id acc_id;
		char dst_uri[512];
		unsigned options;
		char repl[512];
		pjsua_msg_data msg_data;
	} make_call_params;	
		
	pj_bool_t valid_sess;					//Indica si la sesion es valida para poder enviar los RMM en el timer Check_CLD_timer
	int _Id;
	pj_pool_t * _Pool;
	pj_sock_t _RdSendSock;
	pj_sockaddr_in _RdSendTo;		
	pj_uint32_t Retardo;					//Retardo en n�mero de muestras (125us)
	pj_mutex_t *circ_buff_mutex;
	pjmedia_circ_buf *p_retbuff;			//Buffer circular para implementar retardo	
	
	pj_bool_t squ_event;					//Indica un evento de squelch on.
	pj_bool_t squ_event_mcast;				//Se activa con el primer squelch de un grupo
											//el numero de secuencia que se envia con los paquetes de audio por multicast	

	pj_bool_t primer_paquete_despues_squelch;

	pj_bool_t squoff_event_mcast;
	unsigned waited_rtp_seq;				//Numero de secuencia esperado por rtp desde la radio
	pj_bool_t hay_retardo;					//Indica si hay retardo despu�s de squelch	
	char zerobuf[256];

	bss_method_types bss_method_type;

	int bss_rx[MAX_BSS_SQU];				//Almacena todos los valores de BSS recibidos desde que se activa el squelch
	int index_bss_rx_w;						//Indice de escritura en bss_rx
	pj_mutex_t *bss_rx_mutex;

	pj_bool_t _Pertenece_a_grupo_FD_con_calculo_retardo_requerido;		
											//Si es true indica que esta sesion pertenece a un grupo de frecuencias desplazadas
											//Y es requerido calculo de retardo. Si es receptor o transceptor y hay mas de un receptor en el grupo incluido este
											//O si es un transmisor y o transceptor y hay mas de un transmisor incluido este
		
	static void window_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te);
											//Callback del timer

	pj_sem_t *sem_out_circbuff;
	pj_thread_t  *out_circbuff_thread;
	pj_bool_t out_circbuff_thread_run;
	static pj_thread_proc Out_circbuff_Th;
	pj_bool_t wait_sem_out_circbuff;		//Indica si tiene que esperar el sem_out_circbuff

	static const int Check_CLD_timer_IDLE = 0;
	static const int Check_CLD_timer_SEND_CLD = 1;
	static const int Check_CLD_timer_SEND_RMM = 2;
	pj_timer_entry Check_CLD_timer;			//Timer para supervisi�n CLD
	static void Check_CLD_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te);
											//Callback del timer

	pj_timer_entry Wait_init_timer;			//Timer que arranca cuando se inicia la sesi�n. Durante ese tiempo se ignora el audio
											//que se recibe de las radios. 
	static void Wait_init_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te);
											//Callback del timer

	pj_timer_entry Wait_fin_timer;			//Timer que arranca cuando se ha finalizado la sesion sip. Al vencer el timer es
											//cuando se hace el delete del objeto sipcall
	static void Wait_fin_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te);
											//Callback del timer

	/*** Necesarios para el calculo de Qidx ****/
	pj_uint8_t last_qidx_value;
	processor_data PdataQidx;				//Datos para el proceso de calculo del QiDx.
	float fPdataQidx[SAMPLES_PER_FRAME*2];
	float afMuestrasIfRx[SAMPLES_PER_FRAME*2];
	float a_dc[2];
	float b_dc[2];
	float fFiltroDC_IfRx_ciX;
	float fFiltroDC_IfRx_ciY;
	/***********/
		
#ifndef _ED137_
	char _RdFr[CORESIP_MAX_RS_LENGTH + 1];		//Identificador de la frecuencia
#endif
	pjsip_evsub * _ConfInfoClientEvSub;
	pjsip_evsub * _ConfInfoSrvEvSub;
	bool _HangUp;

	char LastReason[CORESIP_MAX_REASON_LENGTH + 1];		//Tiene el Reason recibido en el BYE o en el CANCEL

#ifdef _ED137_
	// PlugTest FAA 05/2011
	pj_str_t _Frequency;
	static struct CORESIP_EstablishedOvrCallMembers _EstablishedOvrCallMembers;
	static char _LocalUri[CORESIP_MAX_URI_LENGTH + 1];
	static char _CallSrcUri[CORESIP_MAX_URI_LENGTH + 1];
#endif

	static pjsip_evsub_user _ConfInfoSrvCb;
	static pjsip_evsub_user _ConfInfoClientCb;

private:	
	void CheckParams();
	void SubscribeToConfInfo();
	static pjmedia_clock_callback Multicast_clock_cb;

	static void OnConfInfoSrvStateChanged(pjsip_evsub *sub, pjsip_event *event);
	static void OnConfInfoClientStateChanged(pjsip_evsub *sub, pjsip_event *event);
	static void OnConfInfoClientRxNotify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, 
		pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body);

	static void EliminarRadSessionDelGrupo(SipCall *call);
	static int FlushSessions(pj_str_t *dst, pjsua_call_id except_cid, CORESIP_CallType calltype);
	void IniciaFinSesion();
	void Dispose();

	//Parametros para cuando es un agente simulador de radio
	pj_bool_t _EnviarQidx;		//Indica que tiene que enviarse el qidx con el rtp.

};

#endif
