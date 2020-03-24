#ifndef __CORESIP_CALL_H__
#define __CORESIP_CALL_H__

class SipCall
{
public:
	SipCall(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo);
	SipCall(pjsua_call_id call_id, const CORESIP_CallInfo * info);
	~SipCall();

	static int New(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo);

	static void Hangup(pjsua_call_id call_id, unsigned code);
	static void Answer(pjsua_call_id call_id, unsigned code, bool addToConference);
	static void Hold(pjsua_call_id call_id, bool hold);
	static void Transfer(pjsua_call_id call_id, pjsua_call_id dst_call_id, const char * dst);
	static void Ptt(pjsua_call_id call_id, const CORESIP_PttInfo * info);
	static void Conference(pjsua_call_id call_id, bool conf);
	static void SendConfInfo(pjsua_call_id call_id, const CORESIP_ConfInfo * info);
	static void SendInfoMsg(pjsua_call_id call_id, const char * info);

	static void TransferAnswer(const char * tsxKey, void * txData, void * evSub, unsigned code);
	static void TransferNotify(void * evSub, unsigned code);
	static void SendOptionsMsg(const char * target);

	static void OnRdRtp(void * stream, void * frame, void * codec, unsigned seq);
	static void OnRdInfoChanged(void * stream, pj_uint32_t rtp_ext_info);
	static void OnKaTimeout(void * stream);
	static void OnCreateSdp(int call_id, void * local_sdp, void * incoming_rdata);
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
private:
	int _Id;
	pj_pool_t * _Pool;
	pj_sock_t _RdSendSock;
	pj_sockaddr_in _RdSendTo;
	CORESIP_CallInfo _Info;
#ifndef _ED137_
	char _RdFr[CORESIP_MAX_RS_LENGTH + 1];
#endif
	pjsip_evsub * _ConfInfoClientEvSub;
	pjsip_evsub * _ConfInfoSrvEvSub;
	bool _HangUp;

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
	void SubscribeToConfInfo();

	static void OnConfInfoSrvStateChanged(pjsip_evsub *sub, pjsip_event *event);
	static void OnConfInfoClientStateChanged(pjsip_evsub *sub, pjsip_event *event);
	static void OnConfInfoClientRxNotify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, 
		pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body);

};

#endif