#include "Global.h"
#include "SipCall.h"
#include "Exceptions.h"
#include "SipAgent.h"

static pj_str_t gSubjectHdr = { "Subject", 7 };
static pj_str_t gPriorityHdr = { "Priority", 8 };
static pj_str_t gReferBy = { "Referred-By", 11 };

static pj_str_t gIsubParam = { "isub", 4 };
static pj_str_t gRsParam = { "cd40rs", 6 };

static pj_str_t gSubject[] = { { "IA call", 7 }, { "monitoring", 10 }, { "G/G monitoring", 10 }, { "A/G monitoring", 10 }, { "DA/IDA call", 11}, { "radio", 5 } };
static pj_str_t gPriority[] = { { "emergency", 9 }, { "urgent", 6 }, { "normal", 6 }, { "non-urgent", 10 } };
static pj_str_t gRecvOnly = { "recvonly", 8 };

static const pj_str_t STR_EVENT = { "Event", 5 };
static const pj_str_t STR_CONFERENCE = { "conference", 10 };
static const pj_str_t STR_EXPIRES = { "Expires", 7 };

#ifdef _ED137_
// PlugTest FAA 05/2011
char SipCall::_LocalUri[CORESIP_MAX_URI_LENGTH + 1];
char SipCall::_CallSrcUri[CORESIP_MAX_URI_LENGTH + 1];
//bool SipCall::_LoopClosure=false;
//int SipCall::_ConfiguredCodecs=0;
//int SipCall::_UdpPortForRtpRecording=0;
//char SipCall::_IpRecordingServer[CORESIP_MAX_URI_LENGTH + 1];
// PlugTest FAA 05/2011
// OVR calls
struct CORESIP_EstablishedOvrCallMembers SipCall::_EstablishedOvrCallMembers;
#endif

/**
 * 
*/
pjsip_evsub_user SipCall::_ConfInfoClientCb = 
{
	&SipCall::OnConfInfoClientStateChanged,  
	NULL,
	NULL,
	&SipCall::OnConfInfoClientRxNotify,
	NULL,
	NULL
};

/**
 * 
*/
pjsip_evsub_user SipCall::_ConfInfoSrvCb = 
{
	&SipCall::OnConfInfoSrvStateChanged,  
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

/**
 * 2
*/
SipCall::SipCall(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo)
					:	_Id(PJSUA_INVALID_ID), _Pool(NULL), _RdSendSock(PJ_INVALID_SOCKET), 
						_ConfInfoClientEvSub(NULL), _ConfInfoSrvEvSub(NULL), _HangUp(false)
{
	_Pool = pjsua_pool_create(NULL, 512, 512);
	pj_memcpy(&_Info, info, sizeof(CORESIP_CallInfo));

	pjsua_msg_data msgData;
	pjsua_msg_data_init(&msgData);

	pjsip_generic_string_hdr subject, priority, referBy;

	pjsip_generic_string_hdr_init2(&subject, &gSubjectHdr, &gSubject[info->Type]);
	pj_list_push_back(&msgData.hdr_list, &subject);
	pjsip_generic_string_hdr_init2(&priority, &gPriorityHdr, &gPriority[info->Priority]);
	pj_list_push_back(&msgData.hdr_list, &priority);

	if (outInfo->ReferBy[0] != 0)
	{
		pjsip_generic_string_hdr_init2(&referBy, &gReferBy, &(pj_str(const_cast<char*>(outInfo->ReferBy))));
		pj_list_push_back(&msgData.hdr_list, &referBy);
	}

	try
	{
#ifdef _ED137_
		if (info->Type == CORESIP_CALL_RD || info->Type == CORESIP_CALL_RRC || info->Type == CORESIP_CALL_RXTXRD)
		{
			_Frequency = pj_str(const_cast<char*>(outInfo->RdFr));
		}
#endif
		if (info->Type == CORESIP_CALL_RD)
		{
#ifndef _ED137_
			pj_ansi_strcpy(_RdFr, outInfo->RdFr);
#endif
			if ((info->Flags & CORESIP_CALL_RD_TXONLY) == 0)
			{
				pj_status_t st = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &_RdSendSock);
				PJ_CHECK_STATUS(st, ("ERROR creando socket para el envio de radio por multicast"));

				pj_sockaddr_in_init(&_RdSendTo, &(pj_str(const_cast<char*>(outInfo->RdMcastAddr))), (pj_uint16_t)outInfo->RdMcastPort);
			}
		}

		unsigned options = info->Flags & CORESIP_CALL_CONF_FOCUS ? PJSUA_CALL_CONFERENCE : 0;
		pjsua_acc_id acc = info->AccountId == PJSUA_INVALID_ID ? pjsua_acc_get_default() : info->AccountId & CORESIP_ID_MASK;

		pj_status_t st = pjsua_call_make_call(acc, &(pj_str(const_cast<char*>(outInfo->DstUri))), options, this, &msgData, &_Id);
		PJ_CHECK_STATUS(st, ("ERROR realizando llamada", "(%s)", outInfo->DstUri));		

		//Envía CallStart al grabador
		pjsua_acc_info info_acc;
		pjsua_acc_get_info(acc, &info_acc); 	
		SipAgent::RecCallStart(SipAgent::OUTCOM, info->Priority, &info_acc.acc_uri, &pj_str(const_cast<char*>(outInfo->DstUri)));
	}
	catch (...)
	{
		if (_RdSendSock != PJ_INVALID_SOCKET)
		{
			pj_sock_close(_RdSendSock);
		}
		pj_pool_release(_Pool);
		throw;
	}

}

/**
 * 
*/
SipCall::SipCall(pjsua_call_id call_id, const CORESIP_CallInfo * info)
:	_Id(call_id), _Pool(NULL), _RdSendSock(PJ_INVALID_SOCKET), 
	_ConfInfoClientEvSub(NULL), _ConfInfoSrvEvSub(NULL), _HangUp(false)
{
	_Pool = pjsua_pool_create(NULL, 512, 512);
	pj_memcpy(&_Info, info, sizeof(CORESIP_CallInfo));
}

/**
 * 
*/
SipCall::~SipCall()
{
	if (_RdSendSock != PJ_INVALID_SOCKET)
	{
		pj_sock_close(_RdSendSock);
	}
	if (_ConfInfoClientEvSub)
	{
		pjsip_tx_data * tdata;
		if ((PJ_SUCCESS != pjsip_conf_initiate(_ConfInfoClientEvSub, 0, &tdata)) ||
			(PJ_SUCCESS != pjsip_conf_send_request(_ConfInfoClientEvSub, tdata)))
		{
			pjsip_conf_terminate(_ConfInfoClientEvSub, PJ_FALSE);
		}
	}
	if (_ConfInfoSrvEvSub)
	{
		pjsip_tx_data * tdata;
		pjsip_conf_status info;

		info.version = 0xFFFFFFFF;
		info.state = pj_str("deleted");
		info.users_cnt = 0;

		pjsip_conf_set_status(_ConfInfoSrvEvSub, &info);
		
		if ((PJ_SUCCESS != pjsip_conf_notify(_ConfInfoSrvEvSub, PJSIP_EVSUB_STATE_TERMINATED, NULL, NULL, &tdata)) ||
			(PJ_SUCCESS != pjsip_conf_send_request(_ConfInfoSrvEvSub, tdata)))
		{
			pjsip_conf_terminate(_ConfInfoClientEvSub, PJ_FALSE);
		}
	}

	pj_pool_release(_Pool);
}

/**
 * 1.
*/
int SipCall::New(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo) 
{ 
	SipCall * call = new SipCall(info, outInfo);
	return call->_Id;
}

/**
 * 
 */
void SipCall::Hangup(pjsua_call_id call_id, unsigned code)
{
	pjsua_call * pjcall;
	pjsip_dialog * dlg;

	pj_status_t st = acquire_call("Hangup()", call_id, &pjcall, &dlg);
	PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));

	SipCall * call = (SipCall*)pjcall->user_data;

	if (call)
	{
		if (call->_ConfInfoClientEvSub)
		{
			pjsip_tx_data * tdata;
			if (PJ_SUCCESS == pjsip_conf_initiate(call->_ConfInfoClientEvSub, 0, &tdata))
			{
				pjsip_conf_send_request(call->_ConfInfoClientEvSub, tdata);
			}
		}
		if (call->_ConfInfoSrvEvSub)
		{
			pjsip_tx_data * tdata;
			pjsip_conf_status info;

			info.version = 0xFFFFFFFF;
			info.state = pj_str("deleted");
			info.users_cnt = 0;

			pjsip_conf_set_status(call->_ConfInfoSrvEvSub, &info);

			if (PJ_SUCCESS == pjsip_conf_notify(call->_ConfInfoSrvEvSub, PJSIP_EVSUB_STATE_TERMINATED, NULL, NULL, &tdata))
			{
				pjsip_conf_send_request(call->_ConfInfoSrvEvSub, tdata);
			}
		}

		call->_HangUp = true;
	}

	pjsua_call_info info;
	st = pjsua_call_get_info(call_id, &info);

	pjsip_dlg_dec_lock(dlg);
	st = pjsua_call_hangup(call_id, code, NULL, NULL);
	PJ_CHECK_STATUS(st, ("ERROR finalizando llamada", "[Call=%d] [Code=%d]", call_id, code));	

}

/**
 * 
 */
void SipCall::Answer(pjsua_call_id call_id, unsigned code, bool addToConference)
{
	if (addToConference)
	{
		pjsua_call * call;
		pjsip_dialog * dlg;

		pj_status_t st = acquire_call("Answer()", call_id, &call, &dlg);
		PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));

		dlg->local.contact->focus = PJ_TRUE;
		pjsip_dlg_dec_lock(dlg);
	}

	if (code & (CORESIP_CALL_EC << 16))
	{
		pjsua_call * call;
		pjsip_dialog * dlg;

		pj_status_t st = acquire_call("Answer()", call_id, &call, &dlg);
		PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));

		if (call->user_data)
		{
			CORESIP_CallFlags flags = ((SipCall*)call->user_data)->_Info.Flags;
			((SipCall*)call->user_data)->_Info.Flags = (CORESIP_CallFlags)((int)flags | CORESIP_CALL_EC);
		}

		pjsip_dlg_dec_lock(dlg);
		code &= 0x0000FFFF;
	}

	pj_status_t st = pjsua_call_answer(call_id, code, NULL, NULL);
	PJ_CHECK_STATUS(st, ("ERROR respondiendo llamada", "[Call=%d] [Code=%d]", call_id, code));
}

/**
 * 
 */
void SipCall::Hold(pjsua_call_id call_id, bool hold)
{
	if (hold)
	{
		pj_status_t st = pjsua_call_set_hold(call_id, NULL);
		PJ_CHECK_STATUS(st, ("ERROR aparcando llamada", "[Call=%d]", call_id));		
	}
	else
	{
		pj_status_t st = pjsua_call_reinvite(call_id, PJ_TRUE, NULL, NULL);
		PJ_CHECK_STATUS(st, ("ERROR reanudando llamada", "[Call=%d]", call_id));
	}
}

/**
 * 
 */
void SipCall::Transfer(pjsua_call_id call_id, pjsua_call_id dest_call_id, const char * dst)
{
	/*pj_status_t st = (dest_call_id != PJSUA_INVALID_ID) ?
	pjsua_call_xfer_replaces(call_id, dest_call_id, 0, NULL) :
	pjsua_call_xfer(call_id, &(pj_str(const_cast<char*>(dst))), NULL);
	PJ_CHECK_STATUS(st, ("ERROR en transferencia de llamada", "[Call=%d][DstCall=%d] [dst=%s]", call_id, dest_call_id, dst));*/

	pj_status_t st;
	if (dest_call_id != PJSUA_INVALID_ID) 
		st = pjsua_call_xfer_replaces(call_id, dest_call_id, 0, NULL);
	else
		st = pjsua_call_xfer(call_id, &(pj_str(const_cast<char*>(dst))), NULL);
	PJ_CHECK_STATUS(st, ("ERROR en transferencia de llamada", "[Call=%d][DstCall=%d] [dst=%s]", call_id, dest_call_id, dst));
}

/**
 * 
 */
void SipCall::Ptt(pjsua_call_id call_id, const CORESIP_PttInfo * info)
{
	pj_uint32_t rtp_ext_info = 0;

	PJMEDIA_RTP_RD_EX_SET_PTT_TYPE(rtp_ext_info, info->PttType);

	if (info->PttType)
	{
		PJMEDIA_RTP_RD_EX_SET_PTT_ID(rtp_ext_info, info->PttId);

		if (info->ClimaxCld)
		{
			PJMEDIA_RTP_RD_EX_SET_X(rtp_ext_info, 1);
			/** AGL
			PJMEDIA_RTP_RD_EX_SET_TYPE(rtp_ext_info, 2);
			PJMEDIA_RTP_RD_EX_SET_LENGTH(rtp_ext_info, 6);
			PJMEDIA_RTP_RD_EX_SET_CLD(rtp_ext_info, info->ClimaxCld);
			*/
		}
	}

	pjmedia_stream * stream = pjmedia_session_get_stream(pjsua_call_get_media_session(call_id), 0);
	pjmedia_stream_set_rtp_ext_tx_info(stream, rtp_ext_info);
}

/**
 * 
 */
void SipCall::Conference(pjsua_call_id call_id, bool conf)
{
	static pj_str_t focus_param = { ";isfocus", 8 };

	pjsua_call * pjcall;
	pjsip_dialog * dlg;
	pj_str_t contact = { NULL, 0 };

	pj_status_t st = acquire_call("Conference()", call_id, &pjcall, &dlg);
	PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));

	SipCall * call = (SipCall*)pjcall->user_data;

	if (call)
	{
		if (!conf && call->_ConfInfoSrvEvSub)
		{
			pjsip_tx_data * tdata;
			pjsip_conf_status info;

			info.version = 0xFFFFFFFF;
			info.state = pj_str("deleted");
			info.users_cnt = 0;

			pjsip_conf_set_status(call->_ConfInfoSrvEvSub, &info);

			if (PJ_SUCCESS == pjsip_conf_notify(call->_ConfInfoSrvEvSub, PJSIP_EVSUB_STATE_TERMINATED, NULL, NULL, &tdata))
			{
				pjsip_conf_send_request(call->_ConfInfoSrvEvSub, tdata);
			}
		}

		// pjsua_acc_create_uac_contact(call->_Pool, &contact, call->_Info.AccountId, NULL, conf ? &focus_param : NULL);
		pjsua_acc_create_uac_contact(call->_Pool, &contact, ((call->_Info.AccountId) & CORESIP_ID_MASK), NULL, conf ? &focus_param : NULL);
		st = pjsua_call_reinvite(call_id, PJ_FALSE, &contact, NULL);
	}
	else
	{
		st = PJSIP_ESESSIONSTATE;
	}

	pjsip_dlg_dec_lock(dlg);
	PJ_CHECK_STATUS(st, ("ERROR modificando conferencia", "[Call=%d] [Modi=%s]", call_id, conf ? "Add" : "Remove"));
}

/**
 * 
 */
void SipCall::SendConfInfo(pjsua_call_id call_id, const CORESIP_ConfInfo * conf)
{
	pjsua_call * pjcall;
	pjsip_dialog * dlg;

	pj_status_t st = acquire_call("SendConfInfo()", call_id, &pjcall, &dlg);
	PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));

	SipCall * call = (SipCall*)pjcall->user_data;

	if (call && call->_ConfInfoSrvEvSub)
	{
		pjsip_conf_status info;

		info.version = conf->Version;
		info.state = pj_str(const_cast<char*>(conf->State));
		info.users_cnt = conf->UsersCount;

		for (unsigned i = 0; i < conf->UsersCount; i++)
		{
			info.users[i].id = pj_str(const_cast<char*>(conf->Users[i].Id));
			info.users[i].display = pj_str(const_cast<char*>(conf->Users[i].Name));
			info.users[i].role = pj_str(const_cast<char*>(conf->Users[i].Role));
			info.users[i].state = pj_str(const_cast<char*>(conf->Users[i].State));
		}

		st = pjsip_conf_set_status(call->_ConfInfoSrvEvSub, &info);
		if (st == PJ_SUCCESS)
		{
			pjsip_tx_data * tdata;
			st = pjsip_conf_current_notify(call->_ConfInfoSrvEvSub, &tdata);

			if (st == PJ_SUCCESS)
			{
				st = pjsip_conf_send_request(call->_ConfInfoSrvEvSub, tdata);
			}
		}
	}

	pjsip_dlg_dec_lock(dlg);
	PJ_CHECK_STATUS(st, ("ERROR enviando usuarios de conferencia", "[Call=%d]", call_id));
}

/**
 * 
 */
void SipCall::SendInfoMsg(pjsua_call_id call_id, const char * info)
{
	pjsua_msg_data msgData;
	pjsua_msg_data_init(&msgData);

	msgData.content_type = pj_str("text/plain");
	msgData.msg_body = pj_str(const_cast<char*>(info));

	static pj_str_t method = { "INFO", 4 };
	pjsua_call_send_request(call_id, &method, &msgData);
}

/**
 * 
 */
void SipCall::TransferAnswer(const char * tsxKey, void * txData, void * evSub, unsigned code)
{
	pj_status_t st = pjsua_call_transfer_answer(code, &(pj_str(const_cast<char*>(tsxKey))), 
		(pjsip_tx_data*)txData, (pjsip_evsub*)evSub);
	PJ_CHECK_STATUS(st, ("ERROR en respuesta a peticion de transferencia"));
}

/**
 * 
 */
void SipCall::TransferNotify(void * evSub, unsigned code)
{
	pjsip_evsub_state evSt = (code < 200) ? PJSIP_EVSUB_STATE_ACTIVE : PJSIP_EVSUB_STATE_TERMINATED;
	pj_status_t st = pjsua_call_transfer_notify(code, evSt, (pjsip_evsub*)evSub);
	PJ_CHECK_STATUS(st, ("ERROR notificando estado de la transferencia"));
}

/**
 * 
 */
void SipCall::SendOptionsMsg(const char * target)
{
	pjsip_tx_data * tdata;
	pj_str_t to = pj_str(const_cast<char*>(target));

	pj_status_t st = pjsip_endpt_create_request(pjsua_var.endpt, &pjsip_options_method,
		&to, &pjsua_var.acc[pjsua_acc_get_default()].cfg.id, &to, NULL, NULL, -1, NULL, &tdata);
	PJ_CHECK_STATUS(st, ("ERROR creando mensaje OPTIONS", "[Target=%s]", target));

	st = pjsip_endpt_send_request_stateless(pjsua_var.endpt, tdata, NULL, NULL);
	if (st != PJ_SUCCESS) 
	{
		pjsip_tx_data_dec_ref(tdata);
	}
	PJ_CHECK_STATUS(st, ("ERROR enviando mensaje OPTIONS", "[Target=%s]", target));
}

/**
 * Replica el Audio recibido en el Grupo Multicast asociado a la frecuencia.
 */
void SipCall::OnRdRtp(void * stream, void * frame, void * codec, unsigned seq)
{
	void * session = pjmedia_stream_get_user_data((pjmedia_stream*)stream);
	void * call = pjmedia_session_get_user_data((pjmedia_session*)session);

	if (call)
	{
		SipCall * sipCall = (SipCall*)((pjsua_call*)call)->user_data;

		if (sipCall && (sipCall->_RdSendSock != PJ_INVALID_SOCKET))
		{
			assert(sipCall->_Info.Type == CORESIP_CALL_RD);
			assert((sipCall->_Info.Flags & CORESIP_CALL_RD_TXONLY) == 0);

			pj_ssize_t size = 0;
			char buf[1500];

			if (frame)
			{
				pjmedia_frame * frame_in = (pjmedia_frame*)frame;
				pjmedia_codec * cdc = (pjmedia_codec*)codec;
				pjmedia_frame frame_out;

				frame_out.buf = buf;
				frame_out.size = sizeof(buf);

				pj_status_t st = cdc->op->decode(cdc, frame_in, (unsigned)frame_out.size, PJ_FALSE, &frame_out);
				if (st == PJ_SUCCESS)
				{
					*((unsigned*)((char*)buf + frame_out.size)) = pj_htonl(seq);
					size = (pj_ssize_t)(frame_out.size + sizeof(unsigned));
				}
			}
			else
			{
				buf[0] = RESTART_JBUF;
				size = 1;
			}

			if (size > 0)
			{
				pj_sock_sendto(sipCall->_RdSendSock, buf, &size, 0, &sipCall->_RdSendTo, sizeof(pj_sockaddr_in));
			}
		}
	}
}

/**
 * 
 */
void SipCall::OnRdInfoChanged(void * stream, pj_uint32_t rtp_ext_info)
{
	if (SipAgent::Cb.RdInfoCb)
	{
		void * session = pjmedia_stream_get_user_data((pjmedia_stream*)stream);
		void * call = pjmedia_session_get_user_data((pjmedia_session*)session);

		if (call)
		{
			CORESIP_RdInfo info = { CORESIP_PTT_OFF };

			info.PttType = (CORESIP_PttType)PJMEDIA_RTP_RD_EX_GET_PTT_TYPE(rtp_ext_info);
			info.PttId = (unsigned short)PJMEDIA_RTP_RD_EX_GET_PTT_ID(rtp_ext_info);
			info.Squelch = PJMEDIA_RTP_RD_EX_GET_SQU(rtp_ext_info);
			info.Sct = PJMEDIA_RTP_RD_EX_GET_SCT(rtp_ext_info);
			// AGL. info.Bss = PJMEDIA_RTP_RD_EX_GET_X(rtp_ext_info) && (PJMEDIA_RTP_RD_EX_GET_TYPE(rtp_ext_info) == 1);
			info.Bss = PJMEDIA_RTP_RD_EX_GET_X(rtp_ext_info) /** AGL 140528 TODO. && (PJMEDIA_RTP_RD_EX_GET_TYPE(rtp_ext_info) == 1)*/;
			if (info.Bss)
			{
				info.BssMethod = /** AGL 140528 TODO. PJMEDIA_RTP_RD_EX_GET_BSS_MT(rtp_ext_info)*/0;
				info.BssValue = /** AGL 140528 TODO. PJMEDIA_RTP_RD_EX_GET_BSS_IDX(rtp_ext_info)*/0;
			}

			SipAgent::Cb.RdInfoCb(((pjsua_call*)call)->index | CORESIP_CALL_ID, &info);
		}
	}
}

/**
 * 
 */
void SipCall::OnKaTimeout(void * stream)
{
	if (SipAgent::Cb.KaTimeoutCb)
	{
		void * session = pjmedia_stream_get_user_data((pjmedia_stream*)stream);
		void * call = pjmedia_session_get_user_data((pjmedia_session*)session);

		if (call)
		{
			SipAgent::Cb.KaTimeoutCb(((pjsua_call*)call)->index | CORESIP_CALL_ID);
		}
	}
}

#ifdef _ED137_
/**
 * 
 */
void SipCall::OnCreateSdp(int call_id, void * sdp, void * rdata_arg)
{
	if (call_id != PJSUA_INVALID_ID)
	{
		pjmedia_sdp_session * local_sdp = (pjmedia_sdp_session*)sdp;
		pjsip_rx_data * rdata = (pjsip_rx_data*)rdata_arg;
		SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);

		//SipCall * call = reinterpret_cast<SipCall*>(pjsua_var.calls[call_id].user_data);

		if (call)
		{
			// Plug-Test FAA 05/2011
			// Eliminar los codecs no configurados
			/*
			for (unsigned int j=0; j<local_sdp->media[0]->attr_count;) 
			{
				pjmedia_sdp_attr *a_rtpmap= local_sdp->media[0]->attr[j];
				
				if (a_rtpmap && pj_stricmp(&(pj_str_t(a_rtpmap->name)),&(pj_str("rtpmap")))==0) 
				{
					pjmedia_sdp_rtpmap ar;
					pjmedia_sdp_attr_get_rtpmap(a_rtpmap, &ar);

					if ((call->_Codecs & 1)!=1 && strncmp((char *)ar.pt.ptr, "8",1)==0)	// PCM A-law
					{
						// Remove media attribute
						pjmedia_sdp_attr_remove(&(local_sdp->media[0]->attr_count),local_sdp->media[0]->attr,a_rtpmap);
						// Remove media format of media description
						pjmedia_sdp_media_fmt_remove( &(local_sdp->media[0]->desc.fmt_count),local_sdp->media[0]->desc.fmt,pj_str("8"));
					}
					else if ((call->_Codecs & 2)!=2 && strncmp((char *)ar.pt.ptr, "0",1)==0)	// PCM µ-law
					{
						// Remove media attribute
						pjmedia_sdp_attr_remove(&(local_sdp->media[0]->attr_count),local_sdp->media[0]->attr,a_rtpmap);
						// Remove media format of media description
						pjmedia_sdp_media_fmt_remove( &(local_sdp->media[0]->desc.fmt_count),local_sdp->media[0]->desc.fmt,pj_str("0"));
					}
					else if ((call->_Codecs & 4)!=4 && strncmp((char *)ar.pt.ptr, "15",2)==0)	// G.728
					{
						// Remove media attribute
						pjmedia_sdp_attr_remove(&(local_sdp->media[0]->attr_count),local_sdp->media[0]->attr,a_rtpmap);
						// Remove media format of media description
						pjmedia_sdp_media_fmt_remove( &(local_sdp->media[0]->desc.fmt_count),local_sdp->media[0]->desc.fmt,pj_str("15"));
					}
					else if ((call->_Codecs & 8)!=8 && strncmp((char *)ar.pt.ptr, "18",2)==0)	// G.729
					{
						// Remove media attribute
						pjmedia_sdp_attr_remove(&(local_sdp->media[0]->attr_count),local_sdp->media[0]->attr,a_rtpmap);
						// Remove media format of media description
						pjmedia_sdp_media_fmt_remove( &(local_sdp->media[0]->desc.fmt_count),local_sdp->media[0]->desc.fmt,pj_str("18"));
					}
					else
						j++;
				}
				else
					break;
			}
			*/
			if (call->_Info.Type == CORESIP_CALL_RD || call->_Info.Type == CORESIP_CALL_RRC || call->_Info.Type == CORESIP_CALL_RXTXRD)
			{
				char kap[15], kam[15];
				SipAgent::KeepAliveParams(kap, kam);

				//pjmedia_sdp_attr * a = pjmedia_sdp_attr_create(call->_Pool, "type", 
				//	call->_Coupling ? &(pj_str("coupling")) : &(pj_str("radio")));
				//pjmedia_sdp_media_add_attr(local_sdp->media[0], a); 
				// Plug-Test FAA 05/2011
				pjmedia_sdp_attr * a = pjmedia_sdp_attr_create(call->_Pool, "type", 
					//call->_Info.Coupling ? &(pj_str("coupling")) : (call->_Info.Dir == CORESIP_DIR_RECVONLY ? &(pj_str("Radio-Rxonly")) : &(pj_str("radio"))));
					call->_Info.Flags & CORESIP_CALL_RD_COUPLING ? &(pj_str("coupling")) : (call->_Info.Dir == CORESIP_DIR_RECVONLY ? &(pj_str("Radio-Rxonly")) : &(pj_str("radio"))));
				pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				// Plug-Test FAA 05/2011
				a = pjmedia_sdp_attr_create(call->_Pool, "session-type", 
					(call->_Info.Dir == CORESIP_DIR_RECVONLY ? &(pj_str("Rxonly")) :
					(call->_Info.Dir == CORESIP_DIR_SENDONLY ? &(pj_str("Txonly")) : &(pj_str("TxRx")))));
				pjmedia_sdp_media_add_attr(local_sdp->media[0], a);

				// Plug-Test FAA 05/2011
				// Añadir atributo txrxmode
				//a = pjmedia_sdp_attr_create(call->_Pool, "txrxmode", 
				//	(call->_Dir == CORESIP_DIR_RECVONLY ? &(pj_str("Rx")) :
				//	(call->_Dir == CORESIP_DIR_SENDONLY ? &(pj_str("Tx")) : &(pj_str("TxRx")))));
				//pjmedia_sdp_media_add_attr(local_sdp->media[0], a);

				a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("RSSI")));
				pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				
				// Plug-Test FAA 05/2011
				if ((call->_Info.BssMethods & 8) == 8)	// PSD
				{
					a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("PSD")));
					pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				}
				if ((call->_Info.BssMethods & 4) == 4)	// C/N
				{
					a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("C/N")));
					pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				}
				if ((call->_Info.BssMethods & 2) == 2)	// AGC
				{
					a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("AGC")));
					pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				}

				// Plug-Test FAA 05/2011
				a = pjmedia_sdp_attr_create(call->_Pool, "ptt_rep", &(pj_str("0")));
				pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				a = pjmedia_sdp_attr_create(call->_Pool, "sigtime", &(pj_str("1")));
				pjmedia_sdp_media_add_attr(local_sdp->media[0], a);

				//a = pjmedia_sdp_attr_create(call->_Pool, "interval", &(pj_str("20")));
				//pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				//a = pjmedia_sdp_attr_create(call->_Pool, "sigtime", &(pj_str("1")));
				//pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				//a = pjmedia_sdp_attr_create(call->_Pool, "ptt_rep", &(pj_str("0")));
				//pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				a = pjmedia_sdp_attr_create(call->_Pool, "R2S-KeepAlivePeriod", &(pj_str(kap)));
				pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				a = pjmedia_sdp_attr_create(call->_Pool, "R2S-KeepAliveMultiplier", &(pj_str(kam)));
				pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				if (pj_strlen(&call->_Frequency) > 0)
				{
					a = pjmedia_sdp_attr_create(call->_Pool, "fid", &call->_Frequency);
					pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
				}

				// Plug-Test FAA 05/2011
				// RTPHE Version
				a = pjmedia_sdp_attr_create(call->_Pool, "rtphe", &(pj_str("1")));
				pjmedia_sdp_media_add_attr(local_sdp->media[0], a);

			}
			// Plug-Test FAA 05/2011
			// Override call
			else if (call->_Info.Type == CORESIP_CALL_OVR)
			{
				char szOvrMembers[CORESIP_MAX_OVR_CALLS_MEMBERS * (CORESIP_MAX_URI_LENGTH + 1)];
				pjmedia_sdp_attr * a = pjmedia_sdp_attr_create(call->_Pool, "service", &(pj_str("duplex")));
				pjmedia_sdp_media_add_attr(local_sdp->media[0], a);

				switch (call->_EstablishedOvrCallMembers.MembersCount)
				{
				case 0:
					pj_ansi_sprintf(szOvrMembers, "%s", _LocalUri);
					break;
				case 1:
					pj_ansi_sprintf(szOvrMembers, "%s,%s", _LocalUri,
																	call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member);
					break;
				case 2:
					pj_ansi_sprintf(szOvrMembers, "%s,%s,%s", _LocalUri,
																	call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[1].Member);
					break;
				case 3:
					pj_ansi_sprintf(szOvrMembers, "%s,%s,%s,%s", _LocalUri,
																	call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[1].Member,
																	call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[2].Member);
					break;
				case 4:
					pj_ansi_sprintf(szOvrMembers, "%s,%s,%s,%s,%s", _LocalUri,
																	call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[1].Member,
																	call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[2].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[3].Member);
					break;
				case 5:
					pj_ansi_sprintf(szOvrMembers, "%s,%s,%s,%s,%s,%s", _LocalUri,
																	call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[1].Member,
																	call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[2].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[3].Member,
																	call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[4].Member);
					break;
				default:
					break;
				}

//				if (call->_EstablishedOvrCallMembers.MembersCount > 0)
//				{
					a = pjmedia_sdp_attr_create(call->_Pool, "sid", &(pj_str(szOvrMembers)));
					pjmedia_sdp_media_add_attr(local_sdp->media[0], a);
//				}
			}
			else if (call->_Info.Dir != CORESIP_DIR_SENDRECV)
			{
				pjmedia_sdp_attr * attr = pjmedia_sdp_media_find_attr2(local_sdp->media[0], "sendrecv", NULL);
				pj_strdup2(call->_Pool, &attr->name, (call->_Info.Dir == CORESIP_DIR_RECVONLY ? "recvonly" : "sendonly"));
			}

			return;
		}
		else if (rdata && !SipAgent::EnableMonitoring)
		{
			pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &(pj_str("subject")), NULL);
			if (subject && (pj_stricmp(&subject->hvalue, &gSubject[CORESIP_CALL_IA]) == 0))
			{
				pjmedia_sdp_attr * attr = pjmedia_sdp_media_find_attr2(local_sdp->media[0], "sendrecv", NULL);
				attr->name = gRecvOnly;
			}
		}
		//else if (!rdata)
		//{
		//	rdata = pjsua_var.calls[call_id].rdata;
		//}
	}

	//if (rdata && !_EnableMonitoring)
	//{
	//	pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &(pj_str("subject")), NULL);
	//	if (subject && (pj_stricmp(&subject->hvalue, &gSubject[CORESIP_CALL_IA]) == 0))
	//	{
	//		pjmedia_sdp_attr * attr = pjmedia_sdp_media_find_attr2(local_sdp->media[0], "sendrecv", NULL);
	//		attr->name = gRecvOnly;
	//	}
	//}
}
#else
void SipCall::OnCreateSdp(int call_id, void * local_sdp, void * incoming_rdata)
{
	if (call_id != PJSUA_INVALID_ID)
	{
		pjmedia_sdp_session * sdp = (pjmedia_sdp_session*)local_sdp;
		pjsip_rx_data * rdata = (pjsip_rx_data*)incoming_rdata;
		SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);

		if (call)
		{
			if (call->_Info.Type == CORESIP_CALL_RD)
			{
				char kap[15], kam[15];
				SipAgent::KeepAliveParams(kap, kam);

				/** AGL 140529 Version Anterior..
				pjmedia_sdp_attr * a = pjmedia_sdp_attr_create(call->_Pool, "type", 
					call->_Info.Flags & CORESIP_CALL_RD_COUPLING ? &(pj_str("coupling")) : &(pj_str("radio")));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
				a = pjmedia_sdp_attr_create(call->_Pool, "session-type", 
					(call->_Info.Flags & CORESIP_CALL_RD_RXONLY ? &(pj_str("Rxonly")) :
					(call->_Info.Flags & CORESIP_CALL_RD_TXONLY ? &(pj_str("Txonly")) : &(pj_str("TxRx")))));
				*/

				/** ED137.. B */
				pj_str_t typedata = call->_Info.Flags & CORESIP_CALL_RD_COUPLING ? (pj_str("Coupling")) :
					call->_Info.Flags & CORESIP_CALL_RD_RXONLY ? (pj_str("Radio-Rxonly")) :
					call->_Info.Flags & CORESIP_CALL_RD_TXONLY ? (pj_str("Radio")) : (pj_str("Radio-TxRx"));
				pjmedia_sdp_attr * a = pjmedia_sdp_attr_create(call->_Pool, "type", &typedata);
				pjmedia_sdp_media_add_attr(sdp->media[0], a);

#ifdef _ROHDE_TEST_
				/* AGL 20160224. Para los receptores Rohde*/
				pj_str_t txrxmodedata = call->_Info.Flags & CORESIP_CALL_RD_RXONLY ? (pj_str("Rx"))  : 
					CORESIP_CALL_RD_TXONLY ? (pj_str("Tx")) : (pj_str("TxRx"));
				a = pjmedia_sdp_attr_create(call->_Pool, "txrxmode", &txrxmodedata);
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
				pj_str_t fmtpdata = pj_str("101 0-15");
				a = pjmedia_sdp_attr_create(call->_Pool, "fmtp", &fmtpdata);
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
#else
				a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("RSSI")));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
				a = pjmedia_sdp_attr_create(call->_Pool, "fid", &(pj_str(call->_RdFr)));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
#endif

				a = pjmedia_sdp_attr_create(call->_Pool, "R2S-KeepAlivePeriod", &(pj_str(kap)));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
				a = pjmedia_sdp_attr_create(call->_Pool, "R2S-KeepAliveMultiplier", &(pj_str(kam)));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);

				//a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("AGC")));
				//pjmedia_sdp_media_add_attr(sdp->media[0], a);
				//a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("C/N")));
				//pjmedia_sdp_media_add_attr(sdp->media[0], a);
				//a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("PSD")));
				//pjmedia_sdp_media_add_attr(sdp->media[0], a);
				//a = pjmedia_sdp_attr_create(call->_Pool, "interval", &(pj_str("20")));
				//pjmedia_sdp_media_add_attr(sdp->media[0], a);
				//a = pjmedia_sdp_attr_create(call->_Pool, "sigtime", &(pj_str("1")));
				//pjmedia_sdp_media_add_attr(sdp->media[0], a);
				//a = pjmedia_sdp_attr_create(call->_Pool, "ptt_rep", &(pj_str("0")));
				//pjmedia_sdp_media_add_attr(sdp->media[0], a);

				/** AGL 1420528 */
				pj_str_t rtphe = pj_str("1");
				a = pjmedia_sdp_attr_create(call->_Pool, "rtphe",&rtphe);
				pjmedia_sdp_media_add_attr(sdp->media[0], a);

			}
			else if ((call->_Info.Type == CORESIP_CALL_MONITORING) || 
				(call->_Info.Type == CORESIP_CALL_GG_MONITORING) ||
				(call->_Info.Type == CORESIP_CALL_AG_MONITORING))
			{
				pjmedia_sdp_attr * attr = pjmedia_sdp_media_find_attr2(sdp->media[0], "sendrecv", NULL);
				attr->name = gRecvOnly;
			}
		}
		else if (rdata && !SipAgent::EnableMonitoring)
		{
			pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &gSubjectHdr, NULL);
			if (subject && (pj_stricmp(&subject->hvalue, &gSubject[CORESIP_CALL_IA]) == 0))
			{
				pjmedia_sdp_attr * attr = pjmedia_sdp_media_find_attr2(sdp->media[0], "sendrecv", NULL);
				attr->name = gRecvOnly;
			}
		}
	}
}
#endif

/**
 * Trata los Cambios de estado de una Llamada.
 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__callback.htm#a06e6135aeaa81b32fdc66fa603a0546c
 */
void SipCall::OnStateChanged(pjsua_call_id call_id, pjsip_event * e)
{
	static pj_str_t _gSubjectHdr = { "Reason", 6 };

	pj_assert(SipAgent::Cb.CallStateCb);

	SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);

	if (call)
	{
		pjsua_call_info callInfo;
		pjsua_call_get_info(call_id, &callInfo);

		CORESIP_CallStateInfo stateInfo = { (CORESIP_CallState)callInfo.state, (CORESIP_CallRole)callInfo.role };

		if (call->_HangUp)
		{
			if (callInfo.state == PJSIP_INV_STATE_DISCONNECTED)
			{
				// Debe mandarse al cliente info de la desconexión de la llamada
				SipAgent::Cb.CallStateCb(call_id | CORESIP_CALL_ID, &call->_Info, &stateInfo);				
				delete call;				
			}
			else
			{
				pjsua_call_hangup(call_id, 0, NULL, NULL);
			}
			
			SipAgent::RecCallEnd(Q931_NORMAL_CALL_CLEARING, CALLEND_UNKNOWN);

			return;
		}


		if (callInfo.role == PJSIP_ROLE_UAC)
		{
			if (callInfo.state == PJSIP_INV_STATE_CONFIRMED)
			{
				if (call->_Info.Type == CORESIP_CALL_RD)
				{
					pjmedia_session_info sess_info;

					pjmedia_session * sess = pjsua_call_get_media_session(call_id);
					pjmedia_session_get_info(sess, &sess_info);

					if (sess_info.stream_cnt > 0)
					{
						stateInfo.PttId = (unsigned short)sess_info.stream_info[0].pttId;
						stateInfo.ClkRate = sess_info.stream_info[0].param->info.clock_rate;
						stateInfo.ChannelCount = sess_info.stream_info[0].param->info.channel_cnt;
						stateInfo.BitsPerSample = sess_info.stream_info[0].param->info.pcm_bits_per_sample;
						stateInfo.FrameTime = sess_info.stream_info[0].param->info.frm_ptime;
					}
				}
			}
		}
		else if (callInfo.role == PJSIP_ROLE_UAS)
		{
			////if (callInfo.state == PJSIP_INV_STATE_CONNECTING)
			////{
			//	if ((call->_Id != PJSUA_INVALID_ID) && (call->_Id != call_id))
			//	{
			//		// Call replaced
			//		call = new SipCall(call_id, &call->_TranferInfo);
			//		pjsua_call_set_user_data(call_id, call);
			//	}
			////}
		}

		stateInfo.LastCode = callInfo.last_status;
		//pj_ansi_snprintf(stateInfo.LastReason, sizeof(stateInfo.LastReason) - 1, "%.*s", callInfo.last_status_text.slen, callInfo.last_status_text.ptr);
		if (stateInfo.LastCode == PJSIP_SC_DECLINE)
		{
			// Comprobar si el DECLINE viene como consecuencia de que la pasarela no es el activa
			pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(e->body.tsx_state.src.rdata->msg_info.msg, &_gSubjectHdr, NULL);
			if (subject)
			{
				pj_ansi_snprintf(stateInfo.LastReason, sizeof(stateInfo.LastReason) - 1, "%.*s", 
					subject->hvalue.slen, subject->hvalue);
			}
		}

		stateInfo.LocalFocus = callInfo.local_focus;
		stateInfo.RemoteFocus = callInfo.remote_focus;
		stateInfo.MediaStatus = (CORESIP_MediaStatus)callInfo.media_status;
		stateInfo.MediaDir = (CORESIP_MediaDir)callInfo.media_dir;

		SipAgent::Cb.CallStateCb(call_id | CORESIP_CALL_ID, &call->_Info, &stateInfo);

		if (callInfo.state == PJSIP_INV_STATE_CONFIRMED)
		{
			if (callInfo.remote_focus && !call->_ConfInfoClientEvSub)
			{
				call->SubscribeToConfInfo();
			}
			else if (!callInfo.remote_focus && call->_ConfInfoClientEvSub)
			{
				pjsip_tx_data * tdata;
				if (PJ_SUCCESS == pjsip_conf_initiate(call->_ConfInfoClientEvSub, 0, &tdata))
				{
					pjsip_conf_send_request(call->_ConfInfoClientEvSub, tdata);
				}
			}
			
			SipAgent::RecCallConnected(&callInfo.remote_info);			
		}
		else if (callInfo.state == PJSIP_INV_STATE_DISCONNECTED)
		{
			SipAgent::RecCallEnd(Q931_NORMAL_CALL_CLEARING, CALLEND_DEST);
			delete call;
		}
	}	
}

/**
 * Trata la Notificacion de Llamada Entrante.
 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__callback.htm#a402dc4b89c409507fa69b54494efef10
 */
void SipCall::OnIncommingCall(pjsua_acc_id acc_id, pjsua_call_id call_id, 
										pjsua_call_id replace_call_id, pjsip_rx_data * rdata)
{
	PJ_UNUSED_ARG(acc_id);
	pj_assert(pjsua_call_get_user_data(call_id) == NULL);

	if (SipAgent::Cb.CallIncomingCb)
	{
		CORESIP_CallInfo info = { acc_id | CORESIP_ACC_ID, CORESIP_CALL_UNKNOWN, CORESIP_PR_UNKNOWN };

		pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &gSubjectHdr, NULL);
		pjsip_priority_hdr * priority = (pjsip_priority_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &gPriorityHdr, NULL);

		if (subject)
		{
			for (info.Type = CORESIP_CALL_IA; 
				info.Type < CORESIP_CALL_UNKNOWN && pj_stricmp(&subject->hvalue, &gSubject[info.Type]);
				info.Type = (CORESIP_CallType)((int)info.Type + 1));
		}
		if (priority)
		{
			for (info.Priority = CORESIP_PR_EMERGENCY; 
				info.Priority < CORESIP_PR_UNKNOWN && pj_stricmp(&priority->hvalue, &gPriority[info.Priority]);
				info.Priority = (CORESIP_Priority)((int)info.Priority + 1));
		}

		if ((info.Type == CORESIP_CALL_UNKNOWN) || (info.Priority == CORESIP_PR_UNKNOWN) ||
			((info.Type == CORESIP_CALL_IA) && (info.Priority != CORESIP_PR_URGENT)) ||
			(!PJSIP_URI_SCHEME_IS_SIP(rdata->msg_info.to->uri) && !PJSIP_URI_SCHEME_IS_SIPS(rdata->msg_info.to->uri)) ||
			(!PJSIP_URI_SCHEME_IS_SIP(rdata->msg_info.from->uri) && !PJSIP_URI_SCHEME_IS_SIPS(rdata->msg_info.from->uri)))
		{
			pjsua_call_answer(call_id, PJSIP_SC_BAD_REQUEST, NULL, NULL);
			return;
		}

		SipCall * call = new SipCall(call_id, &info);
		pjsua_call_set_user_data(call_id, call);

		CORESIP_CallInInfo inInfo = { };
		pjsip_sip_uri * src = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.from->uri);
		pjsip_sip_uri * dst = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.to->uri);

		pj_ansi_snprintf(inInfo.SrcId, sizeof(inInfo.SrcId) - 1, "%.*s", src->user.slen, src->user.ptr);
		pj_ansi_snprintf(inInfo.SrcIp, sizeof(inInfo.SrcIp) - 1, "%.*s", src->host.slen, src->host.ptr);
		pj_ansi_snprintf(inInfo.DstId, sizeof(inInfo.DstId) - 1, "%.*s", dst->user.slen, dst->user.ptr);
		pj_ansi_snprintf(inInfo.DstIp, sizeof(inInfo.DstIp) - 1, "%.*s", dst->host.slen, dst->host.ptr);

		pjsip_param * p = pjsip_param_find(&(src->other_param), &gIsubParam);
		if (p != NULL)
		{
			pj_ansi_snprintf(inInfo.SrcSubId, sizeof(inInfo.SrcSubId) - 1, "%.*s", p->value.slen, p->value.ptr);
		}
		p = pjsip_param_find(&(src->other_param), &gRsParam);
		if (p != NULL)
		{
			pj_ansi_snprintf(inInfo.SrcRs, sizeof(inInfo.SrcRs) - 1, "%.*s", p->value.slen, p->value.ptr);
		}
		p = pjsip_param_find(&(dst->other_param), &gIsubParam);
		if (p != NULL)
		{
			pj_ansi_snprintf(inInfo.DstSubId, sizeof(inInfo.DstSubId) - 1, "%.*s", p->value.slen, p->value.ptr);
		}

		//Para extraer la uri de destino
		pjsua_acc_info info_acc_id;
		pjsua_acc_get_info(acc_id, &info_acc_id);

		//Paa extraer la uri origen
		pjsua_call_info info_call_id;
		pjsua_call_get_info(call_id, &info_call_id);

		//Envia mensaje CallStart al grabador
		SipAgent::RecCallStart(SipAgent::INCOM, info.Priority, &info_call_id.remote_contact, &info_acc_id.acc_uri);

		int call2replace = replace_call_id != PJSUA_INVALID_ID ? replace_call_id | CORESIP_CALL_ID : PJSUA_INVALID_ID;
		SipAgent::Cb.CallIncomingCb(call_id | CORESIP_CALL_ID, call2replace, &info, &inInfo);
	}
	else
	{
		pjsua_call_answer(call_id, PJSIP_SC_NOT_IMPLEMENTED, NULL, NULL);
	}
}

/**
 * Trata la conexion Desconexion de la Media a un dispositivo.
 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__callback.htm#a952b89dc85a47af3037c19ef06b776e3
 */
void SipCall::OnMediaStateChanged(pjsua_call_id call_id)
{
	pjsua_call_info callInfo;
	pjsua_call_get_info(call_id, &callInfo);

	if (callInfo.media_status == PJSUA_CALL_MEDIA_ERROR)
	{
		pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, &(pj_str("SDP negotiation failed")), NULL);
	}
	else if (callInfo.state == PJSIP_INV_STATE_CONFIRMED)
	{
		if (SipAgent::Cb.CallStateCb)
		{
			SipCall * call = (SipCall*)(pjsua_call_get_user_data(call_id));

			if (call)
			{
				CORESIP_CallStateInfo stateInfo = { (CORESIP_CallState)callInfo.state, (CORESIP_CallRole)callInfo.role };

				//stateInfo.LastCode = callInfo.last_status;
				//pj_ansi_snprintf(stateInfo.LastReason, sizeof(stateInfo.LastReason) - 1, "%.*s", callInfo.last_status_text.slen, callInfo.last_status_text.ptr);

				stateInfo.LocalFocus = callInfo.local_focus;
				stateInfo.RemoteFocus = callInfo.remote_focus;				
				stateInfo.MediaStatus = (CORESIP_MediaStatus)callInfo.media_status;
				stateInfo.MediaDir = (CORESIP_MediaDir)callInfo.media_dir;

				SipAgent::Cb.CallStateCb(call_id | CORESIP_CALL_ID, &call->_Info, &stateInfo);

				if (callInfo.remote_focus && !call->_ConfInfoClientEvSub)
				{
					call->SubscribeToConfInfo();
				}
				else if (!callInfo.remote_focus && call->_ConfInfoClientEvSub)
				{
					pjsip_tx_data * tdata;
					if (PJ_SUCCESS == pjsip_conf_initiate(call->_ConfInfoClientEvSub, 0, &tdata))
					{
						pjsip_conf_send_request(call->_ConfInfoClientEvSub, tdata);
					}
				}
				
				if (callInfo.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD || 
					callInfo.media_status == PJSUA_CALL_MEDIA_NONE)
				{				
					SipAgent::RecHold(true, true);
				}
				else if (callInfo.media_status == CORESIP_MEDIA_REMOTE_HOLD)
				{
					SipAgent::RecHold(true, false);					
				} 
				else if (callInfo.media_status == PJSUA_CALL_MEDIA_ACTIVE)
				{
					SipAgent::RecHold(false, true);
				}
			}
		}
	}
}

/**
 * Trata la notificación de transferencia de llamada.
 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__callback.htm#a598000260c40a43f81e138829377bd44
 */
void SipCall::OnTransferRequest(pjsua_call_id call_id, 
										  const pj_str_t * refer_to, const pj_str_t * refer_by, 
										  const pj_str_t * tsxKey, pjsip_tx_data * tdata, pjsip_evsub * sub)
{
	SipCall * old_call = (SipCall*)pjsua_call_get_user_data(call_id);
	if (!old_call)
	{
		pjsua_call_transfer_answer(PJSIP_SC_INTERNAL_SERVER_ERROR, tsxKey, tdata, sub);
		return;
	}

	if (old_call->_Info.Type != CORESIP_CALL_DIA)
	{
		pjsua_call_transfer_answer(PJSIP_SC_BAD_REQUEST, tsxKey, tdata, sub);
		return;
	}

	if (SipAgent::Cb.TransferRequestCb)
	{
		CORESIP_CallTransferInfo transferInfo = { tdata, sub };

		pj_ansi_snprintf(transferInfo.TsxKey, sizeof(transferInfo.TsxKey) - 1, "%.*s", tsxKey->slen, tsxKey->ptr);
		pj_ansi_snprintf(transferInfo.ReferTo, sizeof(transferInfo.ReferTo) - 1, "%.*s", refer_to->slen, refer_to->ptr);
		if (refer_by)
		{
			pj_ansi_snprintf(transferInfo.ReferBy, sizeof(transferInfo.ReferBy) - 1, "%.*s", refer_by->slen, refer_by->ptr);
		}

		pj_str_t dup;
		pj_strdup_with_null(old_call->_Pool, &dup, refer_to);

		char * ptr = pj_strchr(&dup, '?');
		if (ptr)
		{
			// Si no quitamos los parametros puede que los parsee mal como uri
			*ptr = '>';
			ptr++;
			*ptr = 0;

			dup.slen = pj_ansi_strlen(dup.ptr);
		}

		pjsip_uri * uri = pjsip_parse_uri(old_call->_Pool, dup.ptr, dup.slen, 0);

		if (PJSIP_URI_SCHEME_IS_SIP(uri) || PJSIP_URI_SCHEME_IS_SIPS(uri))
		{
			pjsip_sip_uri * dst = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);

			pj_ansi_snprintf(transferInfo.DstId, sizeof(transferInfo.DstId) - 1, "%.*s", dst->user.slen, dst->user.ptr);
			pj_ansi_snprintf(transferInfo.DstIp, sizeof(transferInfo.DstIp) - 1, "%.*s", dst->host.slen, dst->host.ptr);

			pjsip_param * p = pjsip_param_find(&(dst->other_param), &gIsubParam);
			if (p != NULL)
			{
				pj_ansi_snprintf(transferInfo.DstSubId, sizeof(transferInfo.DstSubId) - 1, "%.*s", p->value.slen, p->value.ptr);
			}
			p = pjsip_param_find(&(dst->other_param), &gRsParam);
			if (p != NULL)
			{
				pj_ansi_snprintf(transferInfo.DstRs, sizeof(transferInfo.DstRs) - 1, "%.*s", p->value.slen, p->value.ptr);
			}
		}
		else if (PJSIP_URI_SCHEME_IS_TEL(uri))
		{
			pjsip_tel_uri * dst = (pjsip_tel_uri*)pjsip_uri_get_uri(uri);

			pj_ansi_snprintf(transferInfo.DstId, sizeof(transferInfo.DstId) - 1, "%.*s", dst->number.slen, dst->number.ptr);
			pj_ansi_snprintf(transferInfo.DstSubId, sizeof(transferInfo.DstSubId) - 1, "%.*s", dst->isub_param.slen, dst->isub_param.ptr);
		}
		else
		{
			pjsua_call_transfer_answer(PJSIP_SC_BAD_REQUEST, tsxKey, tdata, sub);
			return;
		}

		SipAgent::Cb.TransferRequestCb(call_id | CORESIP_CALL_ID, &old_call->_Info, &transferInfo);
	}
	else
	{
		pjsua_call_transfer_answer(PJSIP_SC_NOT_IMPLEMENTED, tsxKey, tdata, sub);
	}
}

/**
 * Trata los cambios de estado de una llamada previamente transferidad
 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__callback.htm#ac418c176dd412181d07dd54bf246faed
 */
void SipCall::OnTransferStatusChanged(pjsua_call_id based_call_id, int st_code, const pj_str_t *st_text, pj_bool_t final, pj_bool_t *p_cont)
{
	if (SipAgent::Cb.TransferStatusCb)
	{
		SipAgent::Cb.TransferStatusCb(based_call_id | CORESIP_CALL_ID, st_code);
	}
}

/**
 * Trata cambios genericos en el estado de una llamada.
 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__callback.htm#acec485ed428d48a6ca0d28027e5cccde
 */
void SipCall::OnTsxStateChanged(pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e)
{
	const pjsip_method info_method = { PJSIP_OTHER_METHOD, { "INFO", 4 } };

	if ((tsx->role == PJSIP_ROLE_UAS) && (tsx->state == PJSIP_TSX_STATE_TRYING) &&
		(pjsip_method_cmp(&tsx->method, &info_method) == 0))
	{
		/*
		* Handle INFO method.
		*/
		pjsip_dialog * dlg = pjsip_tsx_get_dlg(tsx);
		if (dlg == NULL)
			return;

		/* Answer incoming INFO with 200/OK */
		pjsip_rx_data * rdata = e->body.tsx_state.src.rdata;
		int answer = PJSIP_SC_BAD_REQUEST;
		pjsip_tx_data *tdata;

		if (rdata->msg_info.msg->body) 
		{
			PJ_LOG(3,(__FILE__, "Call %d: incoming INFO:\n%.*s", 
				call_id,
				(int)rdata->msg_info.msg->body->len,
				rdata->msg_info.msg->body->data));


			if (SipAgent::Cb.InfoReceivedCb)
			{
				SipAgent::Cb.InfoReceivedCb(call_id | CORESIP_CALL_ID,(const char *)(rdata->msg_info.msg->body->data),rdata->msg_info.msg->body->len);
			}

			answer = PJSIP_SC_OK;
		} 

		pj_status_t st = pjsip_dlg_create_response(dlg, rdata, answer, NULL, &tdata);
		if (st != PJ_SUCCESS) 
		{
			pjsua_perror("sipcall.cpp", "Unable to create response to INFO", st);
			return;
		}

		st = pjsip_dlg_send_response(dlg, tsx, tdata);
		if (st != PJ_SUCCESS) 
		{
			pjsua_perror("sipcall.cpp", "Unable to send response to INFO", st);
			return;
		}
	}
	else if ((tsx->role == PJSIP_ROLE_UAC) && (tsx->state == PJSIP_TSX_STATE_COMPLETED) &&
		(pjsip_method_cmp(&tsx->method, pjsip_get_refer_method()) == 0))
	{
		if (SipAgent::Cb.TransferStatusCb)
		{
			int code = e->body.tsx_state.src.rdata->msg_info.msg->line.status.code;
			if (code != PJSIP_SC_ACCEPTED)
			{
				SipAgent::Cb.TransferStatusCb(call_id | CORESIP_CALL_ID, code);
			}
		}
	}
	else if ((tsx->role == PJSIP_ROLE_UAS) && (tsx->state == PJSIP_TSX_STATE_TRYING) &&
		(pjsip_method_cmp(&tsx->method, pjsip_get_subscribe_method()) == 0))
	{

		pjsip_rx_data * rdata = e->body.tsx_state.src.rdata;

		pjsip_event_hdr * event = (pjsip_event_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_EVENT, NULL);
		pjsip_expires_hdr *expires = (pjsip_expires_hdr*)pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_EXPIRES, NULL);

		if (event && (pj_stricmp(&event->event_type, &STR_CONFERENCE) == 0) && expires->ivalue != 0)
		{
			pjsip_dialog * dlg = pjsip_tsx_get_dlg(tsx);
			if (dlg == NULL)
				return;

			SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);
			pj_status_t st;

			if (call && dlg->local.contact->focus)
			{
				if (!call->_ConfInfoSrvEvSub)
				{
					st = pjsip_conf_create_uas(dlg, &call->_ConfInfoSrvCb, rdata, &call->_ConfInfoSrvEvSub);
					if (st != PJ_SUCCESS)
					{
						pjsua_perror("sipcall.cpp", "ERROR creando servidor de subscripcion a conferencia", st);
						goto subscription_error;
					}
					pjsip_evsub_set_mod_data(call->_ConfInfoSrvEvSub, pjsua_var.mod.id, call);																			
				}							

				st = pjsip_conf_accept(call->_ConfInfoSrvEvSub, rdata, PJSIP_SC_OK, NULL);
				if (st != PJ_SUCCESS)
				{
					pjsua_perror("sipcall.cpp", "ERROR aceptando subscripcion a conferencia", st);
					goto subscription_error;
				}

				pjsip_evsub_set_state(call->_ConfInfoSrvEvSub, PJSIP_EVSUB_STATE_ACCEPTED);
				//pjsip_evsub *sub = call->_ConfInfoSrvEvSub;
				//call->_ConfInfoSrvEvSub->state = PJSIP_EVSUB_STATE_ACCEPTED;
				//sub->state = PJSIP_EVSUB_STATE_ACCEPTED;
			}

			return;
	
subscription_error:
			pjsip_tx_data * tdata;
			st = pjsip_dlg_create_response(dlg, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, &tdata);
			if (st != PJ_SUCCESS) 
			{
				pjsua_perror("sipcall.cpp", "Unable to create error response to conference subscription", st);
				return;
			}

			st = pjsip_dlg_send_response(dlg, tsx, tdata);
			if (st != PJ_SUCCESS) 
			{
				pjsua_perror("sipcall.cpp", "Unable to send error response to conference subscription", st);
				return;
			}
		}
	}
}

/**
 * Gestiona la creación de los stream y su conexion a los puertos de conferencia
 * http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__callback.htm#a88e61b65e936271603e5d63503aebcf6
 */
void SipCall::OnStreamCreated(pjsua_call_id call_id, pjmedia_session * sess, unsigned stream_idx, pjmedia_port **p_port)
{
	SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);

	if (call && (call->_Info.Flags & CORESIP_CALL_EC))
	{		
		pjmedia_session_enable_ec(sess, stream_idx, SipAgent::EchoTail, SipAgent::EchoLatency, 0);							// AGL. 20131120. Parámetros Configurables.
	}
}

/**
 * 
 */
void SipCall::SubscribeToConfInfo()
{
	pjsua_call * call;
	pjsip_dialog * dlg;

	pj_status_t st = acquire_call("SubscribeToConfInfo", _Id, &call, &dlg);
	if (st != PJ_SUCCESS)
	{
		return;
	}

	try
	{
		st = pjsip_conf_create_uac(dlg, &_ConfInfoClientCb, PJSIP_EVSUB_NO_EVENT_ID, &_ConfInfoClientEvSub);
		PJ_CHECK_STATUS(st, ("ERROR creando cliente para subscripcion a conferencia", "[Call=%d]", _Id));
		pjsip_evsub_set_mod_data(_ConfInfoClientEvSub, pjsua_var.mod.id, this);

		pjsip_tx_data * tdata;
		st = pjsip_conf_initiate(_ConfInfoClientEvSub, -1, &tdata);
		PJ_CHECK_STATUS(st, ("ERROR creando mensaje de subscripcion a conferencia", "[Call=%d]", _Id));

		/* Add additional headers etc */
		pjsua_process_msg_data(tdata, NULL);

		st = pjsip_conf_send_request(_ConfInfoClientEvSub, tdata);
		PJ_CHECK_STATUS(st, ("ERROR enviando subscripcion a conferencia", "[Call=%d]", _Id));
	}
	catch (...)
	{
		if (_ConfInfoClientEvSub != NULL)
		{
			pjsip_conf_terminate(_ConfInfoClientEvSub, PJ_FALSE);
			_ConfInfoClientEvSub = NULL;
		}
	}

	pjsip_dlg_dec_lock(dlg);
}

/**
 * 
 */
void SipCall::OnConfInfoSrvStateChanged(pjsip_evsub *sub, pjsip_event *event)
{
	SipCall * call = (SipCall*)pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
	if (call) 
	{
		PJ_LOG(4, ("sipcall.cpp", "Conference server subscription is %s", pjsip_evsub_get_state_name(sub)));

		if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) 
		{
			call->_ConfInfoSrvEvSub = NULL;
			pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
		} 
	}

	PJ_UNUSED_ARG(event);
}

/**
 * 
 */
void SipCall::OnConfInfoClientStateChanged(pjsip_evsub *sub, pjsip_event *event)
{
	SipCall * call = (SipCall*)pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
	if (call) 
	{
		PJ_LOG(4, ("sipcall.cpp", "Conference client subscription is %s", pjsip_evsub_get_state_name(sub)));

		if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) 
		{
			call->_ConfInfoClientEvSub = NULL;
			pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
		} 
	}

	PJ_UNUSED_ARG(event);
}

/**
 * 
 */
void SipCall::OnConfInfoClientRxNotify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, 
													  pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body)
{
	if (SipAgent::Cb.ConfInfoCb)
	{
		SipCall * call = (SipCall*)pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
		if (call) 
		{
			pjsip_conf_status conf_info;

			if (PJ_SUCCESS == pjsip_conf_get_status(sub, &conf_info))
			{
				CORESIP_ConfInfo info;

				info.Version = conf_info.version;
				info.UsersCount = conf_info.users_cnt;
				pj_ansi_snprintf(info.State, sizeof(info.State) - 1, "%.*s", conf_info.state.slen, conf_info.state.ptr);

				for (unsigned i = 0; i < conf_info.users_cnt; i++)
				{
					pj_ansi_snprintf(info.Users[i].Id, sizeof(info.Users[i].Id) - 1, "%.*s", conf_info.users[i].id.slen, conf_info.users[i].id.ptr);
					pj_ansi_snprintf(info.Users[i].Name, sizeof(info.Users[i].Name) - 1, "%.*s", conf_info.users[i].display.slen, conf_info.users[i].display.ptr);
					pj_ansi_snprintf(info.Users[i].Role, sizeof(info.Users[i].Role) - 1, "%.*s", conf_info.users[i].role.slen, conf_info.users[i].role.ptr);
					pj_ansi_snprintf(info.Users[i].State, sizeof(info.Users[i].State) - 1, "%.*s", conf_info.users[i].state.slen, conf_info.users[i].state.ptr);
				}

				SipAgent::Cb.ConfInfoCb(call->_Id | CORESIP_CALL_ID, &info);
			}
		}
	}

	/* The default is to send 200 response to NOTIFY.
	* Just leave it there..
	*/
	PJ_UNUSED_ARG(rdata);
	PJ_UNUSED_ARG(p_st_code);
	PJ_UNUSED_ARG(p_st_text);
	PJ_UNUSED_ARG(res_hdr);
	PJ_UNUSED_ARG(p_body);
}


#ifdef _ED137_
// PlugTest FAA 05/2011
/**
 * 
 */
pj_status_t SipCall::OnNegociateSdp(void * inv_arg, void * neg_arg, void * rdata_arg)
{
	bool is_rd = false;
	bool is_ovr = false;
	SipCall * call = NULL;

	pjsip_inv_session * inv = (pjsip_inv_session *)inv_arg;
	pjmedia_sdp_neg * neg = (pjmedia_sdp_neg *)neg_arg;
	pjsip_rx_data * rdata = (pjsip_rx_data *)rdata_arg;

	if (inv)
		call = reinterpret_cast<SipCall*>(reinterpret_cast<pjsua_call*>(inv->dlg->mod_data[pjsua_var.mod.id])->user_data);

	if (call)
	{
		is_rd = (call->_Info.Type == CORESIP_CALL_RD || call->_Info.Type == CORESIP_CALL_RRC || call->_Info.Type == CORESIP_CALL_RXTXRD);
		is_ovr = (call->_Info.Type == CORESIP_CALL_OVR);
	}
	//else if (rdata)
	//{
	//	pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &(pj_str("subject")), NULL);
	//	if (subject)
	//	{
	//		is_rd = (pj_stricmp(&subject->hvalue, &gSubject[CORESIP_CALL_RD]) == 0);
	//	}
	//}

	// PlugTest FAA  05/2011
	// rtphe version
	if (is_rd)
	{
		const pjmedia_sdp_session * rem_sdp;
		pjmedia_sdp_neg_get_active_remote(neg, &rem_sdp);

		pjmedia_sdp_attr * a = pjmedia_sdp_media_find_attr2(rem_sdp->media[0], "rtphe", NULL);
		//if (!a || pj_strcmp2(&a->value, "0")==0)
		//	_Cb.OnNegociateSdp(false,call->Id);
		//else
		//	_Cb.OnNegociateSdp(true,call->Id);
	}
	// PlugTest FAA  05/2011
	// Override calls
	else if (is_ovr)
	{
		char szOvrMembers[CORESIP_MAX_OVR_CALLS_MEMBERS * (CORESIP_MAX_URI_LENGTH + 1)];
		const pjmedia_sdp_session * loc_sdp;
		pjmedia_sdp_neg_get_active_local(neg, &loc_sdp);
		pjmedia_sdp_attr * a = pjmedia_sdp_attr_create(call->_Pool, "service", &(pj_str("duplex")));
		pjmedia_sdp_media_add_attr(loc_sdp->media[0], a);

		switch (call->_EstablishedOvrCallMembers.MembersCount)
		{
		case 0:
			pj_ansi_sprintf(szOvrMembers, "%s", _LocalUri);
			break;
		case 1:
			pj_ansi_sprintf(szOvrMembers, "%s,%s", _LocalUri,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member);
			break;
		case 2:
			pj_ansi_sprintf(szOvrMembers, "%s,%s,%s", _LocalUri, call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[1].Member);
			break;
		case 3:
			pj_ansi_sprintf(szOvrMembers, "%s,%s,%s,%s", _LocalUri, call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[1].Member,
															call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[2].Member);
			break;
		case 4:
			pj_ansi_sprintf(szOvrMembers, "%s,%s,%s,%s,%s", _LocalUri, call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[1].Member,
															call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[2].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[3].Member);
			break;
		case 5:
			pj_ansi_sprintf(szOvrMembers, "%s,%s,%s,%s,%s,%s", _LocalUri, call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[0].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[1].Member,
															call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[2].Member,call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[3].Member,
															call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[4].Member);
			break;
		default:
			break;
		}


		//if (call->_EstablishedOvrCallMembers.MembersCount > 0)
		//{
			a = pjmedia_sdp_attr_create(call->_Pool, "sid", &(pj_str(szOvrMembers)));
			pjmedia_sdp_media_add_attr(loc_sdp->media[0], a);
//		}
	
		UpdateInfoSid(call, neg);
	}

	return PJ_SUCCESS;
}

void SipCall::AddOvrMember(SipCall * call, pjsua_call_id call_id, const char * info, bool incommingCall)
{
	int cuantos=call->_EstablishedOvrCallMembers.MembersCount;

	char szBuffer[CORESIP_MAX_URI_LENGTH + 1];

	while (info!=0x00 && *info==' ')
		info++;


	pj_ansi_strcpy(szBuffer,strupr((char *)info));

	char * pstr1=pj_ansi_strchr(szBuffer,'@');
	if (pstr1)
	{
		char * pstr2=pj_ansi_strchr(pstr1,':');
		if (pstr2)
		{
			if (szBuffer[pj_ansi_strlen(szBuffer)-1]=='>')
			{
				*pstr2='>';
				*(++pstr2)=0x00;
			}
			else
				*pstr2=0x00;
		}
	}

	if (call && cuantos < CORESIP_MAX_OVR_CALLS_MEMBERS && pj_ansi_strcmp(szBuffer,_LocalUri)!=0)
	{
		for (short i=0;i<cuantos;i++)
		{
			if (pj_ansi_strcmp(call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[i].Member,szBuffer)==0)
			{
				if (call_id >= 0 && call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[i].CallId < 0)
				{
					call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[i].CallId=call_id;
					//call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[i].IncommingCall=incommingCall;
				}
				return;
			}
		}

		pj_ansi_strcpy(call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[cuantos].Member,szBuffer);
		call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[cuantos].CallId=call_id;
		//if (call_id >=0)
		call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[cuantos].IncommingCall=incommingCall;

		++(call->_EstablishedOvrCallMembers.MembersCount);

		if (SipAgent::Cb.OnUpdateOvrCallMembers != NULL)
			SipAgent::Cb.OnUpdateOvrCallMembers(call->_EstablishedOvrCallMembers);
	}
}


void SipCall::UpdateInfoSid(SipCall * call, pjmedia_sdp_neg * neg)
{
	const pjmedia_sdp_session * rem_sdp;

	pjmedia_sdp_neg_get_active_remote(neg, &rem_sdp);
	pjmedia_sdp_attr *a = pjmedia_sdp_media_find_attr2(rem_sdp->media[0], "sid", NULL);
	if (a)
	{
		char * pstr=pj_ansi_strstr(a->value.ptr,",");
		char * origen=a->value.ptr;
		while (origen - a->value.ptr < a->value.slen)
		{
			char szBuf[CORESIP_MAX_OVR_CALLS_MEMBERS * (CORESIP_MAX_URI_LENGTH + 1)];
			pj_ansi_sprintf(szBuf,"%.*s",pstr ? pstr - origen : (a->value.slen - (origen - a->value.ptr)),origen);

			AddOvrMember(call, call->_Id, szBuf, pj_ansi_strstr(szBuf,_CallSrcUri)!=NULL);

			//call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[call->_EstablishedOvrCallMembers.MembersCount].CallId=-1;
			//pj_ansi_strcpy(call->_EstablishedOvrCallMembers.EstablishedOvrCallMembers[call->_EstablishedOvrCallMembers.MembersCount].Member,szBuf);
			//call->_EstablishedOvrCallMembers.MembersCount++;

			if (pstr)
			{
				origen=++pstr;
				pstr=pj_ansi_strstr(origen,",");
			}
			else
				break;
		}
	}
}
#endif
