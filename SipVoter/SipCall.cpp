#include "Global.h"
#include "SipCall.h"
#include "Exceptions.h"
#include "SipAgent.h"
#include "processor.h"
#include "ExtraParamAccId.h"

static pj_str_t gSubjectHdr = { "Subject", 7 };
static pj_str_t gPriorityHdr = { "Priority", 8 };
static pj_str_t gReferBy = { "Referred-By", 11 };
static pj_str_t gRequire = { "Require", 7 };
static pj_str_t gReplaces = { "Replaces", 8 };

/** ED137B */
pj_str_t SipCall::gWG67VersionName = {"WG67-Version", 12};
pj_str_t SipCall::gWG67VersionRadioValue = {"radio.01", 8};
pj_str_t SipCall::gWG67VersionTelefValue = {"phone.01", 8};
/** Reason Header */

static pj_str_t gWG67ReasonName = {"Reason", 6};
static pj_str_t gWG67Reason2000 = {"WG-67; cause=2000; text=\"session pre-emption\"", 45};
static pj_str_t gWG67Reason2001 = {"WG-67; cause=2001; text=\"missing R2S KeepAlive\"", 47};
static pj_str_t gWG67Reason2002 = {"WG-67; cause=2002; text=\"fid does not match\"", 44};
static pj_str_t gWG67Reason2003 = {"WG-67; cause=2003; text=\"radio in maintenance mode\"", 51};
static pj_str_t gWG67Reason2004 = {"WG-67; cause=2004; text=\"internal error\"", 40};
static pj_str_t gWG67Reason2005 = {"WG-67; cause=2005; text=\"coupling not allowed\"", 46};
static pj_str_t gWG67Reason2006 = {"WG-67; cause=2006; text=\"radio access mode doesn�t match\"", 57};
static pj_str_t gWG67Reason2007 = {"WG-67; cause=2007; text=\"parameter error\"", 41};
static pj_str_t gWG67Reason2008 = {"WG-67; cause=2008; text=\"limit exceeded\"", 40};
static pj_str_t gWG67Reason2009 = {"WG-67; cause=2009; text=\"linked session clear command\"", 54};
static pj_str_t gWG67Reason2010 = {"WG-67; cause=2010; text=\"linked session disabled\"", 49};
/** */

/** */
static unsigned int WG67Reason = 0;
static pj_str_t gIsubParam = { "isub", 4 };
static pj_str_t gRsParam = { "cd40rs", 6 };

static pj_str_t gSubject[] = { { "IA call", 7 }, { "monitoring", 10 }, { "G/G monitoring", 14 }, { "A/G monitoring", 14 }, { "DA/IDA call", 11}, { "radio", 5 } };
// static pj_str_t gSubject[] = { { "ia call", 7 }, { "monitoring", 10 }, { "g/g monitoring", 14 }, { "a/g monitoring", 14 }, { "da/ida call", 11}, { "radio", 5 } };
static pj_str_t gPriority[] = { { "emergency", 9 }, { "urgent", 6 }, { "normal", 6 }, { "non-urgent", 10 } };
static pj_str_t gRecvOnly = { "recvonly", 8 };
static pj_str_t gSendOnly = { "sendonly", 8 };

static const pj_str_t STR_EVENT = { "Event", 5 };
static const pj_str_t STR_CONFERENCE = { "conference", 10 };
static const pj_str_t STR_EXPIRES = { "Expires", 7 };

static bool TestRadioIdle = false;

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

/** */
pj_str_t *getWG67ReasonContent() 
{
	return WG67Reason==2000 ? (pj_str_t *) &gWG67Reason2000 :
		WG67Reason==2001 ? (pj_str_t *) &gWG67Reason2001 :
		WG67Reason==2003 ? (pj_str_t *) &gWG67Reason2003 :
		WG67Reason==2004 ? (pj_str_t *) &gWG67Reason2004 :
		WG67Reason==2005 ? (pj_str_t *) &gWG67Reason2005 :
		WG67Reason==2006 ? (pj_str_t *) &gWG67Reason2006 :
		WG67Reason==2007 ? (pj_str_t *) &gWG67Reason2007 :
		WG67Reason==2008 ? (pj_str_t *) &gWG67Reason2008 :
		WG67Reason==2009 ? (pj_str_t *) &gWG67Reason2009 :
		WG67Reason==2010 ? (pj_str_t *) &gWG67Reason2010 : (pj_str_t *) NULL;
}


/** */
void SetPreferredCodec(int codec)
{
	if (codec==0)
	{
		pjsua_codec_set_priority(&(pj_str(const_cast<char*>("PCMA"))), PJMEDIA_CODEC_PRIO_HIGHEST);
		pjsua_codec_set_priority(&(pj_str(const_cast<char*>("PCMU"))), PJMEDIA_CODEC_PRIO_NEXT_HIGHER);
		pjsua_codec_set_priority(&(pj_str(const_cast<char*>("G728"))), PJMEDIA_CODEC_PRIO_NORMAL);
	}
	else if (codec==1)
	{
		pjsua_codec_set_priority(&(pj_str(const_cast<char*>("PCMA"))), PJMEDIA_CODEC_PRIO_NEXT_HIGHER);
		pjsua_codec_set_priority(&(pj_str(const_cast<char*>("PCMU"))), PJMEDIA_CODEC_PRIO_HIGHEST);
		pjsua_codec_set_priority(&(pj_str(const_cast<char*>("G728"))), PJMEDIA_CODEC_PRIO_NORMAL);
	}
	else if (codec==2)
	{
		pjsua_codec_set_priority(&(pj_str(const_cast<char*>("PCMA"))), PJMEDIA_CODEC_PRIO_NEXT_HIGHER);
		pjsua_codec_set_priority(&(pj_str(const_cast<char*>("PCMU"))), PJMEDIA_CODEC_PRIO_NORMAL);
		pjsua_codec_set_priority(&(pj_str(const_cast<char*>("G728"))), PJMEDIA_CODEC_PRIO_HIGHEST);
	}
	else if (codec == 0xFF)
	{
		//No cambiamos el codec por defecto que se configuro al inicializar la coresip
	}
}

/** */
void SipCall::Wg67VersionSet(pjsip_tx_data *txdata, pj_str_t *valor)
{
	if (pjsip_msg_find_hdr_by_name(txdata->msg, &pj_str("WG67-Version"), NULL)==NULL) 
	{
		pjsip_generic_string_hdr *pWg67version = pjsip_generic_string_hdr_create(txdata->pool, &gWG67VersionName, valor);
		pj_list_push_back(&txdata->msg->hdr, pWg67version);
	}
}

/** */
void Wg67ReasonSet(pjsip_tx_data *txdata) 
{
	//pj_str_t *wg67r;

	if (pjsip_msg_find_hdr_by_name(txdata->msg, &pj_str("Reason"), NULL)==NULL) 
	{
		//wg67r = getWG67ReasonContent();
		pjsip_generic_string_hdr *pWg67Reason = pjsip_generic_string_hdr_create(txdata->pool, &gWG67ReasonName, getWG67ReasonContent());
		//pjsip_generic_string_hdr *pWg67Reason = pjsip_generic_string_hdr_create(txdata->pool, &gWG67ReasonName, wg67r);
		pj_list_push_back(&txdata->msg->hdr, pWg67Reason);
	}
}

/** */
void Wg67ContactSet(pjsip_tx_data *txdata) 
{
	if (pjsip_msg_find_hdr_by_name(txdata->msg, &pj_str("Contact"), NULL)==NULL) 
	{
		pjsip_contact_hdr *contact = pjsip_contact_hdr_create(txdata->pool);
		contact->uri = (pjsip_uri*)SipAgent::pContacUrl;
		pjsip_msg_add_hdr(txdata->msg, (pjsip_hdr*)contact); 
	}
}

/** */
void Wg67AllowSet(pjsip_tx_data *txdata)
{
	if (pjsip_msg_find_hdr_by_name(txdata->msg, &pj_str("Allow"), NULL)==NULL) 
	{
		pjsip_allow_hdr *allow = pjsip_allow_hdr_create(txdata->pool);
		allow->count=0;
		pjsip_msg_add_hdr(txdata->msg, (pjsip_hdr*)allow);
	}
}

/** */
void Wg67SupportedSet(pjsip_tx_data *txdata)
{
	if (pjsip_msg_find_hdr_by_name(txdata->msg, &pj_str("Supported"), NULL)==NULL) 
	{
		pjsip_supported_hdr *supported = pjsip_supported_hdr_create(txdata->pool);
		supported->count=0;
		pjsip_msg_add_hdr(txdata->msg, (pjsip_hdr*)supported);
	}
}

/** */
void Wg67RadioPrioritySet(pjsip_tx_data *txdata) 
{
	if (pjsip_msg_find_hdr_by_name(txdata->msg, &pj_str("Priority"), NULL)==NULL) 
	{
		pjsip_generic_string_hdr *pPriority = pjsip_generic_string_hdr_create(txdata->pool, &gPriorityHdr, &gPriority[2]);
		pj_list_push_back(&txdata->msg->hdr, pPriority);
	}
}

/** */
void Wg67RadioSubjectSet(pjsip_tx_data *txdata) 
{
	if (pjsip_msg_find_hdr_by_name(txdata->msg, &pj_str("Subject"), NULL)==NULL) 
	{
		pjsip_generic_string_hdr *pSubject = pjsip_generic_string_hdr_create(txdata->pool, &gSubjectHdr, &gSubject[5]);
		pj_list_push_back(&txdata->msg->hdr, pSubject);
	}
}

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

std::atomic_int SipCall::SipCallCount = {0};

/**
* 2
*/
SipCall::SipCall(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo)
	:	_Id(PJSUA_INVALID_ID), _Pool(NULL), _RdSendSock(PJ_INVALID_SOCKET), 
	_ConfInfoClientEvSub(NULL), _ConfInfoSrvEvSub(NULL), _HangUp(false)
{	
	SipCall::SipCallCount++;

	_Pool = NULL;
	DstUri[0] = '\0';
	_Index_group = FrecDesp::INVALID_GROUP_INDEX;
	_Index_sess = FrecDesp::INVALID_SESS_INDEX;
	_Sending_Multicast_enabled = PJ_FALSE;
	memset(&RdInfo_prev, 0, sizeof(RdInfo_prev));
	squ_status = SQU_OFF;
	_Pertenece_a_grupo_FD_con_calculo_retardo_requerido = PJ_FALSE;
	LastReason[0] = '\0';

	valid_sess = PJ_FALSE;
	_Id = PJSUA_INVALID_ID;
	_RdSendSock = PJ_INVALID_SOCKET;
	Retardo = 0;
	circ_buff_mutex = NULL;
	p_retbuff = NULL;
	squ_event = PJ_FALSE;
	squ_event_mcast = PJ_FALSE;
	squoff_event_mcast = PJ_FALSE;
	waited_rtp_seq = 0;
	hay_retardo = PJ_FALSE;
	memset(zerobuf, 0, sizeof(zerobuf));
	index_bss_rx_w = 0;
	bss_rx_mutex = NULL;
	RdInfo_prev_mutex = NULL;
	sem_out_circbuff = NULL;
	out_circbuff_thread_run = PJ_FALSE;
	out_circbuff_thread = NULL;
	wait_sem_out_circbuff = PJ_TRUE;
	_EnviarQidx = PJ_FALSE;
	primer_paquete_despues_squelch = PJ_TRUE;
	bss_method_type = NINGUNO;

	last_qidx_value = 0;
	a_dc[0] = 1.0f;
	a_dc[1] = -0.9950f;
	b_dc[0] = 0.9975f;
	b_dc[1] = -0.9975f;
	fFiltroDC_IfRx_ciX = 0.0f;
	fFiltroDC_IfRx_ciY = 0.0f;
	processor_init(&PdataQidx, 0);

	try
	{			
		pj_memcpy(&_Info, info, sizeof(CORESIP_CallInfo));

		if (_Info.porcentajeRSSI < MIN_porcentajeRSSI || _Info.porcentajeRSSI > MAX_porcentajeRSSI)
		{
			pj_status_t st = PJ_EINVAL;
			PJ_CHECK_STATUS(st, ("ERROR PesoRSSIvsNucleo no valido"));
		}

		if (strcmp(info->bss_method, "Ninguno") == 0)
		{
			bss_method_type = NINGUNO;
		}
		else if (strcmp(info->bss_method, "RSSI") == 0)
		{
			bss_method_type = RSSI;
		}
		else if (strcmp(info->bss_method, "RSSI_NUCLEO") == 0)
		{
			bss_method_type = RSSI_NUC;
		}
		else if (strcmp(info->bss_method, "CENTRAL") == 0)
		{
			bss_method_type = CENTRAL;
		}

		/***¡¡¡¡ De momento ignoramos el parámetro de zona !!!!!***/
		/*Para ignorarlo forzamos que ese parametro sea siempre el mismo*/
		strcpy((char *) _Info.Zona, (const char *) "ZONA_DEF");
		
		_Pool = pjsua_pool_create(NULL, 4096*2, 4096);
		//_Pool = pjsua_pool_create(NULL, 65536, 65536);
			
		if (_Pool == NULL)
		{
			pj_status_t st = PJ_ENOMEM;
			PJ_CHECK_STATUS(st, ("ERROR creando pjsua pool en SipCall"));
		}

		if (_Info.cld_supervision_time > 5 || _Info.cld_supervision_time < 0) _Info.cld_supervision_time = 2;
		//Si cld_supervision_time vale 0 entonces no hay supervision de CLD

		//pjsua_msg_data msgData;
		pjsua_msg_data_init(&make_call_params.msg_data);

		//pjsip_generic_string_hdr subject, priority, referBy;
	
		pjsip_generic_string_hdr_init2(&make_call_params.subject, &gSubjectHdr, &gSubject[info->Type]);
		pj_list_push_back(&make_call_params.msg_data.hdr_list, &make_call_params.subject);
		pjsip_generic_string_hdr_init2(&make_call_params.priority, &gPriorityHdr, &gPriority[info->Priority]);
		pj_list_push_back(&make_call_params.msg_data.hdr_list, &make_call_params.priority);

		if (outInfo->ReferBy[0] != 0)
		{
			pjsip_generic_string_hdr_init2(&make_call_params.referBy, &gReferBy, &(pj_str(const_cast<char*>(outInfo->ReferBy))));
			pj_list_push_back(&make_call_params.msg_data.hdr_list, &make_call_params.referBy);
		}

		if (outInfo->RequireReplaces)
		{			
			if ((strlen(outInfo->CallIdToReplace)+strlen(";from-tag=")+strlen(outInfo->FromTag)+strlen("to-tag=")+
				strlen(outInfo->ToTag)+strlen(";early-only")) < (sizeof(make_call_params.repl)/sizeof(char)))
			{
				//nos aseguramos que cabe en el array
				pjsip_generic_string_hdr_init2(&make_call_params.require, &gRequire, &(pj_str("replaces")));
				pj_list_push_back(&make_call_params.msg_data.hdr_list, &make_call_params.require);	

				strcpy(make_call_params.repl, outInfo->CallIdToReplace);
				strcat(make_call_params.repl,";from-tag=");
				strcat(make_call_params.repl, outInfo->FromTag);
				strcat(make_call_params.repl, ";to-tag=");
				strcat(make_call_params.repl, outInfo->ToTag);
				if (outInfo->EarlyOnly) strcat(make_call_params.repl, ";early-only");

				pj_str_t strrepl = pj_str(make_call_params.repl);
				pjsip_generic_string_hdr_init2(&make_call_params.replaces, &gReplaces, &strrepl);
				pj_list_push_back(&make_call_params.msg_data.hdr_list, &make_call_params.replaces);
			}	
			else
			{
				PJ_LOG(3,(__FILE__, "ERROR: SipCall No se puede construir cabecera Replaces"));
			}
		}
						
		pj_timer_entry_init( &window_timer, 0, NULL, window_timer_cb);
		pj_timer_entry_init( &Check_CLD_timer, Check_CLD_timer_IDLE, NULL, Check_CLD_timer_cb);		
		pj_timer_entry_init( &Wait_init_timer, 0, NULL, Wait_init_timer_cb);		
		pj_timer_entry_init( &Ptt_off_timer, 0, NULL, Ptt_off_timer_cb);
		pj_timer_entry_init( &Wait_fin_timer, 0, NULL, Wait_fin_timer_cb);

#ifdef _ED137_
		if (info->Type == CORESIP_CALL_RD || info->Type == CORESIP_CALL_RRC || info->Type == CORESIP_CALL_RXTXRD)
		{
			_Frequency = pj_str(const_cast<char*>(outInfo->RdFr));
		}
#endif

		if (info->Type == CORESIP_CALL_RD)
		{
			SipAgent::IsRadio=true;
#ifndef _ED137_
			pj_ansi_strcpy(_RdFr, outInfo->RdFr);
#endif
			if ((info->Flags & CORESIP_CALL_RD_TXONLY) == 0)
			{
				pj_status_t st = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &_RdSendSock);
				PJ_CHECK_STATUS(st, ("ERROR creando socket para el envio de radio por multicast"));

				//Se fuerza que los paquetes salgan por el interfaz que utiliza el agente.
				struct pj_in_addr in_uaIpAdd;
				pj_str_t str_uaIpAdd;
				str_uaIpAdd.ptr = SipAgent::Get_uaIpAdd();
				str_uaIpAdd.slen = strlen(SipAgent::Get_uaIpAdd());
				pj_inet_aton((const pj_str_t *) &str_uaIpAdd, &in_uaIpAdd);
				st = pj_sock_setsockopt(_RdSendSock, pj_SOL_IP(), PJ_IP_MULTICAST_IF, (void *)&in_uaIpAdd, sizeof(in_uaIpAdd));	
				if (st != PJ_SUCCESS)
					PJ_LOG(3,(__FILE__, "ERROR: setsockopt, PJ_IP_MULTICAST_IF. El envio de audio a %s:%d no se puede forzar por el interface %s", 
						outInfo->RdMcastAddr, outInfo->RdMcastPort, SipAgent::Get_uaIpAdd()));
						
				pj_sockaddr_in_init(&_RdSendTo, &(pj_str(const_cast<char*>(outInfo->RdMcastAddr))), (pj_uint16_t)outInfo->RdMcastPort);
			}
		}

		/** AGL. CODEC Preferido en la LLamada. */
		SetPreferredCodec(info->PreferredCodec);

		unsigned options = info->Flags & CORESIP_CALL_CONF_FOCUS ? PJSUA_CALL_CONFERENCE : 0;
		pjsua_acc_id acc = info->AccountId == PJSUA_INVALID_ID ? pjsua_acc_get_default() : info->AccountId & CORESIP_ID_MASK;
		
		pj_status_t st = pj_mutex_create_simple(_Pool, "RdInfo_prev_mutex", &RdInfo_prev_mutex);
		PJ_CHECK_STATUS(st, ("ERROR creando mutex RdInfo_prev_mutex"));

		if (_Info.Type == CORESIP_CALL_RD)
		{
			st = pj_mutex_create_simple(_Pool, "CircBuffMtx", &circ_buff_mutex);
			PJ_CHECK_STATUS(st, ("ERROR creando mutex para buffer circular"));

			//st = pjmedia_circ_buf_create(_Pool, 16384*8, &p_retbuff);
			st = pjmedia_circ_buf_create(_Pool, 4096*2, &p_retbuff);
			PJ_CHECK_STATUS(st, ("ERROR Creando buffer circular"));	
			pjmedia_circ_buf_reset(p_retbuff);	
			
			st = pj_mutex_create_simple(_Pool, "bss_rx_mutex", &bss_rx_mutex);
			PJ_CHECK_STATUS(st, ("ERROR creando mutex bss_rx_mutex"));

			//Inicializa el timer de la ventana de decision. Este timer solo se arranca si el squelch de esta radio
			//es el primero que se activa y si est� en un grupo bss
			pj_timer_entry_init( &window_timer, 0, (void *) this, window_timer_cb);

			st = pj_sem_create(_Pool, "sem_out_circbuff", 0, 10, &sem_out_circbuff);
			PJ_CHECK_STATUS(st, ("ERROR creando sem_out_circbuff"));

			out_circbuff_thread_run = PJ_TRUE;
			st = pj_thread_create(_Pool, "Out_circbuff_Th", &Out_circbuff_Th, this, 0, 0, &out_circbuff_thread);
			PJ_CHECK_STATUS(st, ("ERROR creando thread out_circbuff_thread"));			
		}
		else
		{
			options |= PJSIP_INV_SUPPORT_TIMER;
			options |= PJSIP_INV_SUPPORT_100REL;
		}

		strncpy(DstUri, outInfo->DstUri, sizeof(DstUri));
		DstUri[sizeof(DstUri) - 1] = '\0';

		//Env�a CallStart al grabador
		pjsua_acc_info info_acc;
		pjsua_acc_get_info(acc, &info_acc); 	
		if (info->Type != CORESIP_CALL_RD)
		{
			SipAgent::RecINVTel();
			SipAgent::RecCallStart(SipAgent::OUTCOM, info->Priority, &info_acc.acc_uri, &pj_str(const_cast<char*>(outInfo->DstUri)));
		}
		else
		{
			SipAgent::RecINVRad();
		}

		if (info->Flags & CORESIP_CALL_EXTERNA)
			options |= PJSIP_INV_VIA_PROXY;

		make_call_params.acc_id = acc;
		strncpy(make_call_params.dst_uri, outInfo->DstUri, sizeof(make_call_params.dst_uri));
		make_call_params.dst_uri[sizeof(make_call_params.dst_uri)-1] = '\0';
		make_call_params.options = options;
	}
	catch (...)
	{
		PJ_LOG(3,(__FILE__, "ERROR: EXCEPCION EN EL CONTRUCTOR SipCall"));
		SipCall::SipCallCount--;		
		Dispose();
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
	SipCall::SipCallCount += 1;

	_Pool = NULL;
	_RdSendSock = PJ_INVALID_SOCKET;
	_Index_group = FrecDesp::INVALID_GROUP_INDEX;
	_Index_sess = FrecDesp::INVALID_SESS_INDEX;
	valid_sess = PJ_FALSE;
	_Pertenece_a_grupo_FD_con_calculo_retardo_requerido = PJ_FALSE;
	LastReason[0] = '\0';

	_Pool = pjsua_pool_create(NULL, 512, 512);
	pj_memcpy(&_Info, info, sizeof(CORESIP_CallInfo));

	DstUri[0] = '\0';
	squ_status = SQU_OFF;
	squ_event = PJ_FALSE;
	squ_event_mcast = PJ_FALSE;	
	squoff_event_mcast = PJ_FALSE;
	Retardo = 0;
	out_circbuff_thread = NULL;
	out_circbuff_thread_run = PJ_FALSE;
	p_retbuff = NULL;
	wait_sem_out_circbuff = PJ_FALSE;
	_Sending_Multicast_enabled = PJ_FALSE;
	pj_timer_entry_init( &window_timer, 0, NULL, window_timer_cb);
	pj_timer_entry_init( &Check_CLD_timer, Check_CLD_timer_IDLE, NULL, Check_CLD_timer_cb);
	pj_timer_entry_init( &Wait_init_timer, 0, NULL, Wait_init_timer_cb);
	pj_timer_entry_init( &Ptt_off_timer, 0, NULL, Ptt_off_timer_cb);
	pj_timer_entry_init( &Wait_fin_timer, 0, NULL, Wait_fin_timer_cb);
	sem_out_circbuff = NULL;
	circ_buff_mutex = NULL;
	bss_rx_mutex = NULL;
	bss_method_type = NINGUNO;
	pj_status_t st = pj_mutex_create_simple(_Pool, "RdInfo_prev_mutex", &RdInfo_prev_mutex);
	PJ_CHECK_STATUS(st, ("ERROR creando mutex RdInfo_prev_mutex"));
	_EnviarQidx = PJ_FALSE;
	memset(&RdInfo_prev, 0, sizeof(RdInfo_prev));

	primer_paquete_despues_squelch = PJ_TRUE;
	waited_rtp_seq = 0;

	last_qidx_value = 0;
	a_dc[0] = 1.0f;
	a_dc[1] = -0.9950f;
	b_dc[0] = 0.9975f;
	b_dc[1] = -0.9975f;
	fFiltroDC_IfRx_ciX = 0.0f;
	fFiltroDC_IfRx_ciY = 0.0f;
	processor_init(&PdataQidx, 0);
}

/**
* 
*/
SipCall::~SipCall()
{
	SipCall::SipCallCount--;
//#ifdef _DEBUG
//	PJ_LOG(3,(__FILE__, "DESTRUCTOR SipCall callid %d SipCall::SipCallCount %d", _Id, SipCall::SipCallCount));
//#else
//	PJ_LOG(5,(__FILE__, "DESTRUCTOR SipCall callid %d SipCall::SipCallCount %d", _Id, SipCall::SipCallCount));
//#endif
	
	Dispose();
}


void SipCall::Dispose()
{	
	Wait_fin_timer.id = 0;
	pjsua_cancel_timer(&Wait_fin_timer);

	Ptt_off_timer.id = 0;
	pjsua_cancel_timer(&Ptt_off_timer);

	Check_CLD_timer.id = Check_CLD_timer_IDLE;
	pjsua_cancel_timer(&Check_CLD_timer);

	Wait_init_timer.id = 0;
	pjsua_cancel_timer(&Wait_init_timer);

	window_timer.id = 0;
	pjsua_cancel_timer(&window_timer);

	if (out_circbuff_thread != NULL && sem_out_circbuff != NULL)
	{
		out_circbuff_thread_run = PJ_FALSE;
		pj_sem_post(sem_out_circbuff);
		pj_thread_join(out_circbuff_thread);
		pj_thread_destroy(out_circbuff_thread);
		out_circbuff_thread = NULL;		
	}

	if (sem_out_circbuff != NULL)
	{
		pj_sem_destroy(sem_out_circbuff);
		sem_out_circbuff = NULL;
	}

	if (circ_buff_mutex != NULL)
	{
		pj_mutex_destroy(circ_buff_mutex);
		circ_buff_mutex = NULL;
	}

	if (_RdSendSock != PJ_INVALID_SOCKET)
	{
		pj_sock_close(_RdSendSock);
		_RdSendSock = PJ_INVALID_SOCKET;
	}

	if (bss_rx_mutex != NULL)
	{
		pj_mutex_destroy(bss_rx_mutex);
		bss_rx_mutex = NULL;
	}

	if (RdInfo_prev_mutex != NULL)
	{
		pj_mutex_destroy(RdInfo_prev_mutex);
		RdInfo_prev_mutex = NULL;
	}

	if (_Pool != NULL)	pj_pool_release(_Pool);
}

int SipCall::Hacer_la_llamada_saliente()
{
	pj_status_t st = pjsua_call_make_call(make_call_params.acc_id, &(pj_str(const_cast<char*>(make_call_params.dst_uri))), 
			make_call_params.options, this, &make_call_params.msg_data, &_Id);	
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: pjsua_call_make_call retorna error"));
		_Id = PJSUA_INVALID_ID;
		return -1;
	}

	return 0;
}

/**
* 1.
*/
int SipCall::New(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo) 
{ 	
	SipCall * call = new SipCall(info, outInfo);
	int ret = PJSUA_INVALID_ID;
	if (call != NULL)
	{
		if (call->Hacer_la_llamada_saliente() != 0)
		{
			delete call;
		}
		else
		{
			ret = call->_Id;
		}		
	}

	if (ret == PJSUA_INVALID_ID)
	{
		char dsturi[512];
		strncpy(dsturi, outInfo->DstUri, sizeof(dsturi));
		dsturi[sizeof(dsturi)-1] = '\0';
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("SipCall::New:", "Error llamando a %s", dsturi);
	}

	return ret;
}

/**
* FlushSessions: Elimina todas las sesiones que esten abiertas para un destino
* @param dst. URI del destino cuyas sesiones queremos eliminar
* @param except_cid. Sería un call id que no queremos eliminar. Si vale PJSUA_INVALID_ID, entonces no hay excepciones
* @param calltype. Tipo de las llamadas
* @return -1 si hay error. 0 no hay error.
*/
int SipCall::FlushSessions(pj_str_t *dst, pjsua_call_id except_cid, CORESIP_CallType calltype)
{
	int ret = 0;
	unsigned calls_count = PJSUA_MAX_CALLS;
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];

	pj_status_t s1 = pjsua_enum_calls(call_ids, &calls_count);
	if (s1 != PJ_SUCCESS) return -1;
	if (calls_count == 0) return 0;


	pj_pool_t* pool = pjsua_pool_create(NULL, 256, 32);
	if (pool == NULL)
	{				
		return -1;
	}

	/*Se crea un string duplicado para el parse, ya que se ha visto que
	pjsip_parse_uri puede modificar el parametro de entrada*/	
	pj_str_t uri_dup;
	pj_strdup_with_null(pool, &uri_dup, dst);

	pjsip_uri *dst_uri = pjsip_parse_uri(pool, uri_dup.ptr, uri_dup.slen, 0);	
	if (dst_uri == NULL)
	{
		pj_pool_release(pool);
		return -1;
	}

	int total_esperas = 8;
	int esperas = total_esperas;
	pj_bool_t fantasma_encontrada = PJ_TRUE;

	//Intentamos cerrar cada una de las sesiones abiertas
	while (esperas > 0 && fantasma_encontrada)
	{
		fantasma_encontrada = PJ_FALSE;
		for (unsigned i = 0; i < calls_count; i++)
		{
			if (call_ids[i] != except_cid)
			{
				CORESIP_CallType Type_call = CORESIP_CALL_UNKNOWN;
				pjsua_call * pjcall;
				pjsip_dialog * dlg;
				pj_status_t st = acquire_call("FlushSessions()", call_ids[i], &pjcall, &dlg);	
				if (st == PJ_SUCCESS)
				{
					SipCall * call = (SipCall*)pjcall->user_data;
					if (call != NULL)
					{
						Type_call = call->_Info.Type;
					}
					pjsip_dlg_dec_lock(dlg);
				}

				pjsua_call_info info;
				if ((calltype == Type_call) && (pjsua_call_get_info(call_ids[i], &info) == PJ_SUCCESS))
				{
					/*Se crea un string duplicado para el parse, ya que se ha visto que
					pjsip_parse_uri puede modificar el parametro de entrada*/	
					pj_str_t uri_dup1;
					pj_strdup_with_null(pool, &uri_dup1, &info.remote_info);

					pjsip_uri *rem_uri = pjsip_parse_uri(pool, uri_dup1.ptr, uri_dup1.slen, 0);		
					if (rem_uri != NULL)
					{
						pj_status_t stus = pjsip_uri_cmp(PJSIP_URI_IN_FROMTO_HDR, dst_uri, rem_uri);
						if (stus == PJ_SUCCESS)
						{
							//Existe una sesion abierta con el mismo destino.
							//Forzamos a cerrar la sesion
							fantasma_encontrada = PJ_TRUE; 
							char buf[256];						
							pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, rem_uri, buf, sizeof(buf)-1);
							PJ_LOG(3,(__FILE__, "WARNING: Parece hay sesion fantasma abierta para este destino %s.", buf));

							//Si encuentra una sesion abierta hacia ese destino entonces espera a que se cierre.
							//En caso de que falten total_esperas/2 entonces se fuerza el hangup

							if (esperas == total_esperas/2)
							{
								PJ_LOG(3,(__FILE__, "WARNING: Parece hay sesion fantasma abierta para este destino %s. Se fuerza su cierre", buf));
								Force_Hangup(call_ids[i], 0);
							}
							pj_thread_sleep(20);
						}
					}
				}
			}
		}
		if (fantasma_encontrada)
		{
			esperas--;		
			pj_status_t s1 = pjsua_enum_calls(call_ids, &calls_count);
			if (s1 != PJ_SUCCESS) 
			{
				ret = -1;
				fantasma_encontrada = PJ_FALSE;		//SE fuerza salga del bucle
			}
		}
	}

	pj_pool_release(pool);
	return ret;
}

void SipCall::IniciaFinSesion()
{
	//Retrasamos el delete del objeto para asegurarnos que han finalizado las transacciones sip
	Wait_fin_timer.cb = Wait_fin_timer_cb;
	Wait_fin_timer.user_data = (void *) this;
	pj_time_val	delay1;
	delay1.sec = (long) 0;
	delay1.msec = (long) (pjsip_cfg()->tsx.tsx_tout * 2);	
	Wait_fin_timer.id = 1;
	pj_status_t st = pjsua_schedule_timer(&Wait_fin_timer, &delay1);
	if (st != PJ_SUCCESS)
	{
		Wait_fin_timer.id = 0;
		PJ_CHECK_STATUS(st, ("ERROR en Wait_fin_timer"));
	}
}

void SipCall::Wait_fin_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
	SipCall *wp = (SipCall *)te->user_data;
	
	pjsua_cancel_timer(&wp->Wait_fin_timer);

	pj_bool_t reiniciar_timer = PJ_FALSE;
	//Finaliza las subscripciones al evento de conferencia si no han sido terminadas ya. Siempre y cuando este agente no 
	//haya sido el que inicia el fin de la sesion, es decir si _HangUp es false
	if (wp->_Info.Type != CORESIP_CALL_RD && !wp->_HangUp && wp->Wait_fin_timer.id == 1)
	{
		//En este caso ya se ha finalizado las subscripciones al evento de presencia en la llamada a hangUp
		if (wp->_ConfInfoClientEvSub)
		{
			if (pjsip_evsub_get_state(wp->_ConfInfoClientEvSub) != PJSIP_EVSUB_STATE_TERMINATED)
			{
				pjsip_tx_data * tdata;
				if ((PJ_SUCCESS != pjsip_conf_initiate(wp->_ConfInfoClientEvSub, 0, &tdata)) ||
					(PJ_SUCCESS != pjsip_conf_send_request(wp->_ConfInfoClientEvSub, tdata)))
				{
					pjsip_conf_terminate(wp->_ConfInfoClientEvSub, PJ_FALSE);
				}
				reiniciar_timer = PJ_TRUE;
			}
		}
		if (wp->_ConfInfoSrvEvSub)
		{
			if (pjsip_evsub_get_state(wp->_ConfInfoSrvEvSub) != PJSIP_EVSUB_STATE_TERMINATED)
			{
				pjsip_tx_data * tdata;
				pjsip_conf_status info;

				info.version = 0xFFFFFFFF;
				info.state = pj_str("deleted");
				info.users_cnt = 0;

				pjsip_conf_set_status(wp->_ConfInfoSrvEvSub, &info);

				if ((PJ_SUCCESS != pjsip_conf_notify(wp->_ConfInfoSrvEvSub, PJSIP_EVSUB_STATE_TERMINATED, NULL, NULL, &tdata)) ||
					(PJ_SUCCESS != pjsip_conf_send_request(wp->_ConfInfoSrvEvSub, tdata)))
				{
					pjsip_conf_terminate(wp->_ConfInfoSrvEvSub, PJ_FALSE);
				}
				reiniciar_timer = PJ_TRUE;
			}
		}
	}

	if (reiniciar_timer)
	{
		//Se reinicia el timer para esperar el fin de las subscripciones al evento de conferencia
		wp->Wait_fin_timer.cb = Wait_fin_timer_cb;
		wp->Wait_fin_timer.user_data = (void *) wp;
		pj_time_val	delay1;
		delay1.sec = (long) 0;
		delay1.msec = (long) (pjsip_cfg()->tsx.tsx_tout * 2);	
		wp->Wait_fin_timer.id = 2;	//Le asignamos un valor diferente a 1 para que solo se reinicie el timer una vez
		pj_status_t st = pjsua_schedule_timer(&wp->Wait_fin_timer, &delay1);
		if (st != PJ_SUCCESS)
		{
			wp->Wait_fin_timer.id = 0;
			PJ_CHECK_STATUS(st, ("ERROR en Wait_fin_timer"));
		}
	}
	else
	{		
		//Eliminamos el objeto
		delete wp;	
	}
}

/**
* 
*/
void SipCall::Hangup(pjsua_call_id call_id, unsigned code)
{
	pjsua_call * pjcall;
	pjsip_dialog * dlg;
	SipCall *call = NULL;	
	
	pj_status_t st;

	if (call_id<0 || call_id>(int)pjsua_var.ua_cfg.max_calls)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Hangup:", "call_id %d no valido", call_id);		
	}

#if 0
	//Buscamos sesiones con el mismo destino que pudieran estar tambien abiertas, cuando son radios
	//o lineas calientes. Si estuvieran abiertas serian sesiones fantasmas que cerraremos
	CORESIP_CallType Type_call = CORESIP_CALL_UNKNOWN;
	char DstUri[CORESIP_MAX_URI_LENGTH + 1];

	st = acquire_call("Hangup()", call_id, &pjcall, &dlg);
	PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));
	call = (SipCall*)pjcall->user_data;
	if (call != NULL)
	{
		Type_call = call->_Info.Type;
		strncpy(DstUri, call->DstUri, sizeof(DstUri)-1);
		DstUri[sizeof(DstUri)-1] = 0;
	}
	pjsip_dlg_dec_lock(dlg);

	if (Type_call == CORESIP_CALL_RD || Type_call == CORESIP_CALL_IA)
	{
		pj_str_t dsturi;
		dsturi.ptr = (char *) DstUri;
		dsturi.slen = strlen(DstUri);
		FlushSessions(&dsturi, call_id, Type_call);		//Se cierran las posibles sesiones a una misma uri
												//de destino menos el de este call_id
	}
#endif

	//Se procede a cerrar la sesion de este call id
	st = acquire_call("Hangup()", call_id, &pjcall, &dlg);
	PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));

	call = (SipCall*)pjcall->user_data;

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

		/*if (call->_Info.Type == CORESIP_CALL_RD)
		{
			pjsua_media_channel_deinit(call_id);
		}*/

		call->_HangUp = true;
	}
		
	pjsip_dlg_dec_lock(dlg);

	//code = PJSIP_SC_BAD_EVENT;

	if (code > 999)
	{
		//codigos de protocolo SIP tiene valores menores de 1000
		WG67Reason = code;
	}

	if (code == 0 || code > 999)
	{
		st = pjsua_call_hangup(call_id, code, NULL, NULL);
	}
	else
	{
		//Si code es distinto de cero se pone la cabecera Reason con el codigo de error
		pjsua_msg_data msg_data;
		pjsua_msg_data_init(&msg_data);
		pjsip_generic_string_hdr reason_hdr;
		pj_str_t reason = pj_str("Reason");		
		char str_reason_val[18];
		strcpy(str_reason_val, "SIP;cause=");
		char str_code[8];
		pj_utoa((unsigned long) code, str_code);
		strncat(str_reason_val, str_code, sizeof(str_reason_val) / sizeof(char));
		str_reason_val[(sizeof(str_reason_val) / sizeof(char))-1] = '\0';
		pj_str_t reason_val = pj_str(str_reason_val);
		pjsip_generic_string_hdr_init2(&reason_hdr, &reason, &reason_val);
		pj_list_push_back(&msg_data.hdr_list, &reason_hdr);
	
		st = pjsua_call_hangup(call_id, code, NULL, &msg_data);
	}
	

	PJ_CHECK_STATUS(st, ("ERROR finalizando llamada", "[Call=%d] [Code=%d]", call_id, code));	
}

/**
* 
*/
int SipCall::Force_Hangup(pjsua_call_id call_id, unsigned code)
{
	pjsua_call * pjcall;
	pjsip_dialog * dlg;

	pj_status_t st = acquire_call("Force_Hangup()", call_id, &pjcall, &dlg);
	if (st != PJ_SUCCESS) return -1;

	SipCall * call = (SipCall*)pjcall->user_data;
	if (!call) 
	{
		pjsip_dlg_dec_lock(dlg);
		return -1;
	}

	call->_HangUp = true;
	pjsip_dlg_dec_lock(dlg);
	
	WG67Reason = code;
	st = pjsua_call_hangup(call_id, code, NULL, NULL);

	if (st != PJ_SUCCESS) return -1;
	return 0;
}

/**
* 
*/
void SipCall::Answer(pjsua_call_id call_id, unsigned code, bool addToConference)
{
	pjsua_call * call1;
	pjsip_dialog * dlg1;

	if (call_id<0 || call_id>(int)pjsua_var.ua_cfg.max_calls)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Answer:", "call_id %d no valido", call_id);		
	}

	pj_status_t status = acquire_call("Answer()", call_id, &call1, &dlg1);
	PJ_CHECK_STATUS(status, ("ERROR adquiriendo call", "[Call=%d]", call_id));
	pjsip_dlg_dec_lock(dlg1);

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
	pjsua_call * pjcall;
	pjsip_dialog * dlg;
	int tries = 5;

	if (call_id<0 || call_id>(int)pjsua_var.ua_cfg.max_calls)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Hold:", "call_id %d no valido", call_id);		
	}

	pj_status_t st = acquire_call("Hold()", call_id, &pjcall, &dlg);
	PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));

	SipCall * call = (SipCall*)pjcall->user_data;
	if (call->_Info.Type == CORESIP_CALL_RD)
	{
		/** 
		Para la Prueba de ReInvite. 
		hold => false RxTx, true RxIdle
		*/
		TestRadioIdle = hold;
		pj_status_t st = pjsua_call_reinvite(call_id, PJ_FALSE, NULL, NULL);
		pjsip_dlg_dec_lock(dlg);
		PJ_CHECK_STATUS(st, ("ERROR reanudando llamada", "[Call=%d]", call_id));
		TestRadioIdle = false;
	}
	else 
	{
		pjsua_msg_data msgData;
		pjsip_generic_string_hdr subject, priority;

		pjsua_msg_data_init(&msgData);

		pjsip_generic_string_hdr_init2(&subject, &gSubjectHdr, &gSubject[call->_Info.Type]);
		pj_list_push_back(&msgData.hdr_list, &subject);
		pjsip_generic_string_hdr_init2(&priority, &gPriorityHdr, &gPriority[call->_Info.Priority]);
		pj_list_push_back(&msgData.hdr_list, &priority);			

		tries = (int) pjsip_sip_cfg_var.tsx.tsx_tout / 20;
		if (tries <= 1) tries = 2;
		while ((pjcall->inv->state != PJSIP_INV_STATE_NULL &&
			pjcall->inv->state != PJSIP_INV_STATE_CONFIRMED) 
			||
			(pjcall->inv->invite_tsx!=NULL))
		{
			//Hay una transacción de invite pendiente. Posiblemente porque el colateral 
			//tarda en contestar. 
			//Por tanto en este bucle se espera un maximo de pjsip_sip_cfg_var.tsx.tsx_tout (ms)
			//a que termine una trasaccion anterior

			pjsip_dlg_dec_lock(dlg);
			pj_thread_sleep(20);

			st = acquire_call("Hold()", call_id, &pjcall, &dlg);
			PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));

			if (--tries == 0) break;
		}		

		if (hold)
		{			
			st = pjsua_call_set_hold(call_id, &msgData);	
			pjsip_dlg_dec_lock(dlg);		
			PJ_CHECK_STATUS(st, ("ERROR aparcando llamada", "[Call=%d]", call_id));		
		}
		else
		{
			st = pjsua_call_reinvite(call_id, PJ_TRUE, NULL, &msgData);
			pjsip_dlg_dec_lock(dlg);	
			PJ_CHECK_STATUS(st, ("ERROR reanudando llamada", "[Call=%d]", call_id));
		}

#if 0
		pjsip_dlg_dec_lock(dlg);
		if (hold)
		{	
			st = pjsua_call_set_hold(call_id, &msgData);	
			if (st == PJ_SUCCESS)
			{	
				//Espera a que pase a estado HOLD
				tries = (int) pjsip_sip_cfg_var.tsx.tsx_tout / 20;
				if (tries <= 1) tries = 2;
				for (;;)
				{
					st = acquire_call("Hold()", call_id, &pjcall, &dlg);
					PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));						

					if (pjcall->media_st == PJSUA_CALL_MEDIA_LOCAL_HOLD ||
						pjcall->media_st == PJSUA_CALL_MEDIA_REMOTE_HOLD)
					{
						pjsip_dlg_dec_lock(dlg);
						break;
					}
					else
					{
						pjsip_dlg_dec_lock(dlg);
						pj_thread_sleep(20);
						if (--tries == 0) break;
					}
				}
			}		
			PJ_CHECK_STATUS(st, ("ERROR aparcando llamada", "[Call=%d]", call_id));		
		}
		else
		{
			st = pjsua_call_reinvite(call_id, PJ_TRUE, NULL, &msgData);
			if (st == PJ_SUCCESS)
			{
				//Espera a abandonar el estado HOLD
				tries = 20;
				for (;;)
				{
					st = acquire_call("Hold()", call_id, &pjcall, &dlg);
					PJ_CHECK_STATUS(st, ("ERROR adquiriendo call", "[Call=%d]", call_id));						

					if (pjcall->media_st != PJSUA_CALL_MEDIA_LOCAL_HOLD &&
						pjcall->media_st != PJSUA_CALL_MEDIA_REMOTE_HOLD)
					{
						pjsip_dlg_dec_lock(dlg);
						break;
					}
					else
					{
						pjsip_dlg_dec_lock(dlg);
						pj_thread_sleep(10);
						if (--tries == 0) break;
					}
				}
			}	
			PJ_CHECK_STATUS(st, ("ERROR reanudando llamada", "[Call=%d]", call_id));
		}
#endif

	}

}

/**
* 
*/
void SipCall::Transfer(pjsua_call_id call_id, pjsua_call_id dest_call_id, const char * dst, const char *referto_display_name)
{
	if (call_id<0 || call_id>(int)pjsua_var.ua_cfg.max_calls)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Transfer:", "call_id %d no valido", call_id);		
	}

	pjsua_call * call1;
	pjsip_dialog * dlg1;
	pj_status_t status = acquire_call("Transfer()", call_id, &call1, &dlg1);
	PJ_CHECK_STATUS(status, ("ERROR adquiriendo call", "[Call=%d]", call_id));
	pjsip_dlg_dec_lock(dlg1);

	/*pj_status_t st = (dest_call_id != PJSUA_INVALID_ID) ?
	pjsua_call_xfer_replaces(call_id, dest_call_id, 0, NULL) :
	pjsua_call_xfer(call_id, &(pj_str(const_cast<char*>(dst))), NULL);
	PJ_CHECK_STATUS(st, ("ERROR en transferencia de llamada", "[Call=%d][DstCall=%d] [dst=%s]", call_id, dest_call_id, dst));*/

	pj_status_t st;
	if (dest_call_id != PJSUA_INVALID_ID) 
		st = pjsua_call_xfer_replaces_dispname(call_id, dest_call_id, (char *) referto_display_name, 0, NULL);
	else
		st = pjsua_call_xfer(call_id, &(pj_str(const_cast<char*>(dst))), NULL);
	PJ_CHECK_STATUS(st, ("ERROR en transferencia de llamada", "[Call=%d][DstCall=%d] [dst=%s]", call_id, dest_call_id, dst));
}

/**
* 
*/
void SipCall::Ptt(pjsua_call_id call_id, const CORESIP_PttInfo * info)
{
	if (call_id<0 || call_id>(int)pjsua_var.ua_cfg.max_calls)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Ptt:", "call_id %d no valido", call_id);		
	}

	pj_uint32_t rtp_ext_info = 0;
	pj_uint32_t rtp_ext_info_prev = 0;
	pj_uint8_t cld = 0;

	pjsua_call * call1;
	pjsip_dialog * dlg1;
	pj_status_t status = acquire_call("Ptt()", call_id, &call1, &dlg1);
	PJ_CHECK_STATUS(status, ("ERROR adquiriendo call", "[Call=%d]", call_id));

	SipCall * call = (SipCall*)call1->user_data;

	pjsua_call_info call_info;
	if (pjsua_call_get_info(call_id, &call_info) != PJ_SUCCESS)
	{
		pjsip_dlg_dec_lock(dlg1);
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Ptt:", "ERROR: No se puede obtener call_info. call_id %d", call_id);		
		return;
	}

	pjmedia_session* session = pjsua_call_get_media_session(call_id);
	if (session == NULL) 
	{
		pjsip_dlg_dec_lock(dlg1);
		return; 
	}

	pjmedia_stream * stream = NULL;
	stream = pjmedia_session_get_stream(session, 0);
	if (stream != NULL)
		pjmedia_stream_get_rtp_ext_tx_info(stream, &rtp_ext_info_prev);

	rtp_ext_info = rtp_ext_info_prev;
		
	PJMEDIA_RTP_RD_EX_SET_PTT_TYPE(rtp_ext_info, info->PttType);
	PJMEDIA_RTP_RD_EX_SET_PTT_ID(rtp_ext_info, info->PttId);
	PJMEDIA_RTP_RD_EX_SET_SQU(rtp_ext_info, info->Squ);

	unsigned int PttType_prev = (unsigned int) PJMEDIA_RTP_RD_EX_GET_PTT_TYPE(rtp_ext_info_prev);
	unsigned int PttMute_prev = (unsigned int) PJMEDIA_RTP_RD_EX_GET_PM(rtp_ext_info_prev);
	unsigned int PttMute = info->PttMute ? 1 : 0;

	if (info->PttType == CORESIP_PTT_OFF && PttMute)
	{
		pjsip_dlg_dec_lock(dlg1);
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Ptt:", "ERROR: Ptt Mute no puede activarse con PTT OFF. call_id %d", call_id);
		PttMute = 0;
		return;
	}

	PJMEDIA_RTP_RD_EX_SET_PM(rtp_ext_info, PttMute);	

	pj_bool_t rdAccount = PJ_FALSE;				//Indica si acc_id es un account tipo radio GRS
	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(call_info.acc_id);
	if (extraParamAccCfg != NULL)
	{
		rdAccount = extraParamAccCfg->rdAccount;
	}

	if (SipAgent::_Radio_UA || rdAccount)		
	{		
		//Es un agente radio o es un account del tipo radio GRS

		SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);
		if (call) 
		{			
			if (call->_EnviarQidx)
			{
				if (info->Squ)
				{
					//Es simulador de radio y tenemos que enviar el qidx con el squelch
					PJMEDIA_RTP_RD_EX_SET_X(rtp_ext_info, 1);			
					PJMEDIA_RTP_RD_EX_SET_TYPE(rtp_ext_info, 1);
					PJMEDIA_RTP_RD_EX_SET_LENGTH(rtp_ext_info, 1);
					pj_uint8_t val;
					if (info->RssiQidx > CORESIP_MAX_RSSI_QIDX)
						val = ((pj_uint8_t) CORESIP_MAX_RSSI_QIDX) << ((pj_uint8_t) 3);	
					else
						val = ((pj_uint8_t) info->RssiQidx) << ((pj_uint8_t) 3);
					PJMEDIA_RTP_RD_EX_SET_VALUE(rtp_ext_info, (val));

#ifdef CHECK_QIDX_LOGARITHM
					//Si es un agente tipo radio simulada conectamos el puerto de la pjmedia que genera un tono con la llamada para que envie audio cuando hay squelch
					//Conectamos el tono a la llamada
					pjsua_conf_port_id call_port_id = pjsua_call_get_conf_port(call_id);
					if (call_port_id != PJSUA_INVALID_ID && SipAgent::_Tone_Port_Id != PJSUA_INVALID_ID)
					{
						pjsua_conf_connect(SipAgent::_Tone_Port_Id, call_port_id);
					}
#endif
				}
				else
				{
					//Es simulador de radio y al quitar el squelch dejamos de enviar el qidx
					PJMEDIA_RTP_RD_EX_SET_X(rtp_ext_info, 0);			
					PJMEDIA_RTP_RD_EX_SET_TYPE(rtp_ext_info, 0);
					PJMEDIA_RTP_RD_EX_SET_LENGTH(rtp_ext_info, 0);
					PJMEDIA_RTP_RD_EX_SET_VALUE(rtp_ext_info, (0));

#ifdef CHECK_QIDX_LOGARITHM
					//Si es un agente tipo radio simulada conectamos el puerto de la pjmedia que genera un tono con la llamada para que envie audio cuando hay squelch
					//Conectamos el tono a la llamada
					pjsua_conf_port_id call_port_id = pjsua_call_get_conf_port(call_id);
					if (call_port_id != PJSUA_INVALID_ID && SipAgent::_Tone_Port_Id != PJSUA_INVALID_ID)
					{
						pjsua_conf_disconnect(SipAgent::_Tone_Port_Id, call_port_id);
					}
#endif

				}
			}
		}
	}

	if (call && info->PttType != PttType_prev)
	{
		//Hay un cambio en el estado del PTT. Si es a OFF arrancamos un timer durante el cual no se
		//envian a la aplicacion infos de la radio. Al final del timer se envia la info actual.
		//Si el cambio es a ON entonces y el timer está en marcha se fuerza la llamada de la callback del timer para actualizar la info
		
		pjsua_cancel_timer(&call->Ptt_off_timer);

		call->Ptt_off_timer.cb = Ptt_off_timer_cb;
		call->Ptt_off_timer.user_data = (void *) call;
		pj_time_val	delay1;
		delay1.sec = (long) 0;

		if (info->PttType == CORESIP_PTT_OFF)
		{
			//Arranca el timer para no enviar estados a la aplicacion				
			PJ_LOG(5,(__FILE__, "SipCall::Ptt Arranca timer PTT OFF. dst %s", call->DstUri));

			delay1.msec = (long) SipAgent::_TimeToDiscardRdInfo;	
			call->Ptt_off_timer.id = 1;
			pj_status_t st = pjsua_schedule_timer(&call->Ptt_off_timer, &delay1);
			if (st != PJ_SUCCESS)
			{
				call->Ptt_off_timer.id = 0;
				PJ_CHECK_STATUS(st, ("ERROR en Ptt_off_timer"));
			}
		}
		else if (call->Ptt_off_timer.id != 0)
		{
			//Se fuerza la llamada a la callback para que se actualice el estado de la info en la aplicacion
			//Para ello cancelamos el timer y lo reanudamos con un delay a cero
			PJ_LOG(5,(__FILE__, "SipCall::Ptt Arranca timer PTT OFF. dst %s con delay a 0 para forzar la callback", call->DstUri));
			delay1.msec = (long) 0;	
			pj_status_t st = pjsua_schedule_timer(&call->Ptt_off_timer, &delay1);
			if (st != PJ_SUCCESS)
			{
				call->Ptt_off_timer.id = 0;
				PJ_CHECK_STATUS(st, ("ERROR en Ptt_off_timer"));
			}
		}			
	}
	
	if (stream != NULL)
	{
		pjmedia_stream_set_rtp_ext_tx_info(stream, rtp_ext_info);

		pj_bool_t forzar_KA = PJ_FALSE;

		if (PttMute_prev != PttMute)
		{
			if ((PttType_prev != 0) && (info->PttType != CORESIP_PTT_OFF) && (PttMute == 0))
			{
				//Si no hay cambio el estado de PTT, es PTT ON, y se desactiva el PTT mute
				//Entonces no se envia el KA y el Ptt mute a cero ira en el audio
			}
			else 
			{
				//Forzamos el envio de un keep alive si el estado de Ptt Mute cambia en cualquier otro caso
				forzar_KA = PJ_TRUE;
			}
		}

		/*Si la sesion es hacia un receptor entonces se fuerza un KA cuando hay cambio de PTT*/				
		if (call)
		{
			CORESIP_CallFlags flags = call->_Info.Flags;
			if ((flags & CORESIP_CALL_RD_RXONLY) && (info->PttType != PttType_prev)) 
			{
				forzar_KA = PJ_TRUE;
			}
		}

		if (SipAgent::_Radio_UA || rdAccount)
		{
			//Es un agente radio o es un account del tipo radio GRS

			if (info->PttType != PttType_prev)
			{
				//Somos un agente radio y el PTT ha cambiado de estado. Tenemos que enviar un keepalive invemdiatamente
				forzar_KA = PJ_TRUE;
			}
		}

		if (forzar_KA) pjmedia_stream_force_send_KA_packet(stream);
	}

	pjsip_dlg_dec_lock(dlg1);
}

/**
* 
*/
void SipCall::Conference(pjsua_call_id call_id, bool conf)
{
	if (call_id<0 || call_id>(int)pjsua_var.ua_cfg.max_calls)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Conference:", "call_id %d no valido", call_id);		
	}

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

		/*A�adimos Subject y Priority en los re-invites*/
		pjsua_msg_data msgData;
		pjsip_generic_string_hdr subject, priority;

		pjsua_msg_data_init(&msgData);

		pjsip_generic_string_hdr_init2(&subject, &gSubjectHdr, &gSubject[call->_Info.Type]);
		pj_list_push_back(&msgData.hdr_list, &subject);
		pjsip_generic_string_hdr_init2(&priority, &gPriorityHdr, &gPriority[call->_Info.Priority]);
		pj_list_push_back(&msgData.hdr_list, &priority);

		// pjsua_acc_create_uac_contact(call->_Pool, &contact, call->_Info.AccountId, NULL, conf ? &focus_param : NULL);
		pjsua_acc_create_uac_contact(call->_Pool, &contact, ((call->_Info.AccountId) & CORESIP_ID_MASK), NULL, conf ? &focus_param : NULL);
		st = pjsua_call_reinvite(call_id, PJ_FALSE, &contact, &msgData);
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
	if (call_id<0 || call_id>(int)pjsua_var.ua_cfg.max_calls)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("SendConfInfo:", "call_id %d no valido", call_id);		
	}

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
				PJ_LOG(5, ("sipcall.cpp", "NOTIFY CONF: Envia conference NOTIFY %s", pjsip_evsub_get_state_name(call->_ConfInfoSrvEvSub)));
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
	if (call_id<0 || call_id>(int)pjsua_var.ua_cfg.max_calls)
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("SendInfoMsg:", "call_id %d no valido", call_id);		
	}

	pjsua_call * call1;
	pjsip_dialog * dlg1;
	pj_status_t status = acquire_call("SendInfoMsg()", call_id, &call1, &dlg1);
	PJ_CHECK_STATUS(status, ("ERROR adquiriendo call", "[Call=%d]", call_id));
	pjsip_dlg_dec_lock(dlg1);

	pjsua_msg_data msgData;
	pj_status_t st;
	pjsua_msg_data_init(&msgData);

	msgData.content_type = pj_str("text/plain");
	msgData.msg_body = pj_str(const_cast<char*>(info));

	static pj_str_t method = { "INFO", 4 };
	st = pjsua_call_send_request(call_id, &method, &msgData);
	PJ_CHECK_STATUS(st, ("ERROR enviando INFO", "[Call=%d]", call_id));
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
 * SendOptionsMsg.	...
 * Envia OPTIONS 
 * @param	target				Uri a la que se envia OPTIONS
 * @param	callid				callid que retorna.
 * @param	isRadio		Si tiene valor distinto de cero el agente se identifica como radio. Si es cero, como telefonia.
 *						Sirve principalmente para poner radio.01 o phone.01 en la cabecera WG67-version
 * @param	by_proxy			TRUE si queremos que se envie a través del proxy. Agregara cabecera route
 */
void SipCall::SendOptionsMsg(const char * target, char *callid, int isRadio, pj_bool_t by_proxy)
{
	pjsip_tx_data * tdata;
	pj_str_t to = pj_str(const_cast<char*>(target));
	pjsua_acc_id acc_id = PJSUA_INVALID_ID;
	pj_str_t callId;

	acc_id = pjsua_acc_get_default();
	if (!pjsua_acc_is_valid(acc_id)) return;

	/*Se comprueba si la URI es valida*/
	pj_bool_t urivalida = PJ_TRUE;
	pj_pool_t* pool = pjsua_pool_create(NULL, 256, 32);
	if (pool == NULL)
	{
		PJ_LOG(3,(__FILE__, "ERROR: SendOptionsMsg: No se puede crear pj_pool"));		
		return;
	}

	/*Se crea un string duplicado para el parse, ya que se ha visto que
	pjsip_parse_uri puede modificar el parametro de entrada*/	
	pj_str_t uri_dup;
	pj_strdup_with_null(pool, &uri_dup, &to);
	pjsip_uri *to_uri = pjsip_parse_uri(pool, uri_dup.ptr, uri_dup.slen, 0);
	
	if (to_uri == NULL) 
		urivalida = PJ_FALSE;
	else if (!PJSIP_URI_SCHEME_IS_SIP(to_uri) && 
			!PJSIP_URI_SCHEME_IS_SIPS(to_uri)) 
		urivalida = PJ_FALSE;
	

	if (!urivalida)
	{
		//target no es una uri valida
		PJ_LOG(3,(__FILE__, "ERROR: La URI a la que se intenta enviar OPTIONS no es valida: %s", target));
		pj_pool_release(pool);
		return;
	}

	pj_create_unique_string(pool, &callId);

	pj_status_t st;
	if (callId.slen > (CORESIP_MAX_CALLID_LENGTH-1))
	{
		pj_pool_release(pool);
		st = PJ_EINVAL;
		PJ_CHECK_STATUS(st, ("ERROR creando mensaje OPTIONS. CallId generado es demasiado largo", "[Target=%s]", target));
		return;
	}

	strncpy(callid, callId.ptr, callId.slen);
	callid[callId.slen] = '\0';

	/*Se envia el request*/
	st = pjsip_endpt_create_request(pjsua_var.endpt, &pjsip_options_method,
		&to, &pjsua_var.acc[acc_id].cfg.id, &to, NULL, &callId, -1, NULL, &tdata);

	pj_pool_release(pool);
	PJ_CHECK_STATUS(st, ("ERROR creando mensaje OPTIONS", "[Target=%s]", target));

	if (by_proxy)
	{
		pjsua_acc *acc = &pjsua_var.acc[acc_id];
		if (!pj_list_empty(&acc->route_set)) 
		{
			pjsua_set_msg_route_set(tdata, &acc->route_set);
		}
#if 0
		else if (pj_strlen(&acc->cfg.reg_uri) > 0)
		{
			//Se fuerza la cabecera Route al Proxy
			/* If we want the initial INVITE to travel to specific SIP proxies,
				* then we should put the initial dialog's route set here. The final
				* route set will be updated once a dialog has been established.
				* To set the dialog's initial route set, we do it with something
				* like this:*/
	
			pjsip_route_hdr route_set;
			pjsip_route_hdr *route;
			const pj_str_t hname = { "Route", 5 };
			char uri[256];
		
			//char *uri = "sip:proxy.server;lr";

			if (acc->cfg.reg_uri.slen >= sizeof(uri))
			{
				acc->cfg.reg_uri.slen = sizeof(uri) - 1;
			}
			strncpy(uri, acc->cfg.reg_uri.ptr, acc->cfg.reg_uri.slen);
			uri[acc->cfg.reg_uri.slen] = '\0';
			strcat(uri, ";lr");

			pj_list_init(&route_set);

			route = (pjsip_route_hdr *) pjsip_parse_hdr( tdata->pool, &hname, 
							uri, strlen(uri),
							NULL);
			if (route != NULL)
				pj_list_push_back(&route_set, route);

			pjsua_set_msg_route_set(tdata, &route_set);

			/*
			* Note that Route URI SHOULD have an ";lr" parameter!
			*/
		}
#endif
	}

	//Establecemos aqui la cabecera WG67-version con la version correspondiente.
	//En la funcion OnTxRequest de SipCall.cpp es donde se establece esta cabecera para todos
	//los paquetes SIP, siempre y cuando no se hayan establecido antes, como es este el caso.
	if (isRadio)
	{
		Wg67VersionSet(tdata, &SipCall::gWG67VersionRadioValue);
	}
	else
	{
		Wg67VersionSet(tdata, &SipCall::gWG67VersionTelefValue);
	}

	st = pjsip_endpt_send_request_stateless(pjsua_var.endpt, tdata, NULL, NULL);
	if (st != PJ_SUCCESS) 
	{
		pjsip_tx_data_dec_ref(tdata);
	}
	PJ_CHECK_STATUS(st, ("ERROR enviando mensaje OPTIONS", "[Target=%s]", target));	
}

/**
* Replica el Audio recibido en el Grupo Multicast asociado a la frecuencia.
* Se reciben paquetes de 10ms
*/
void SipCall::OnRdRtp(void * stream, void * frame, void * codec, unsigned seq, pj_uint32_t rtp_ext_info)
{
	void * session = pjmedia_stream_get_user_data((pjmedia_stream*)stream);
	void * call = pjmedia_session_get_user_data((pjmedia_session*)session);	

	if (!call) return;

	SipCall * sipCall = (SipCall*)((pjsua_call*)call)->user_data;
	if (!sipCall) return;

	pjmedia_frame * frame_in = (pjmedia_frame*)frame;		

	if (frame)
	{		
		if (frame_in->size == 80)
		{
			if (sipCall->waited_rtp_seq != seq) 
			{
				if (!sipCall->primer_paquete_despues_squelch)
				{				
					PJ_LOG(3,(__FILE__, "WARNING: RTP PERDIDO seq esperado %u recibido %u dst %s", 
						sipCall->waited_rtp_seq, seq, sipCall->DstUri));									
				}
				else
				{
					sipCall->primer_paquete_despues_squelch = PJ_FALSE;
				}
			}
			sipCall->waited_rtp_seq = seq;
			sipCall->waited_rtp_seq++;			
		}
	}

	if ((sipCall->_RdSendSock != PJ_INVALID_SOCKET) && (sipCall->Wait_init_timer.id == 0) &&
		(sipCall->_Info.Type == CORESIP_CALL_RD) && ((sipCall->_Info.Flags & CORESIP_CALL_RD_TXONLY) == 0))
	{
		//assert(sipCall->_Info.Type == CORESIP_CALL_RD);
		//assert((sipCall->_Info.Flags & CORESIP_CALL_RD_TXONLY) == 0);

		pj_ssize_t size = 0;
		char buf[1500];

		if (frame)
		{
			pjmedia_codec * cdc = (pjmedia_codec*)codec;
			pjmedia_frame frame_out;

			frame_out.buf = buf;
			frame_out.size = sizeof(buf);

			pj_status_t st = cdc->op->decode(cdc, frame_in, (unsigned)frame_out.size, PJ_FALSE, &frame_out);
			if ((st == PJ_SUCCESS) && (frame_out.size == (SAMPLES_PER_FRAME/2) * sizeof(pj_int16_t)))
			{
				int x = PJMEDIA_RTP_RD_EX_GET_X(rtp_ext_info);
				pj_uint32_t type = PJMEDIA_RTP_RD_EX_GET_TYPE(rtp_ext_info);
				pj_uint32_t length = PJMEDIA_RTP_RD_EX_GET_LENGTH(rtp_ext_info);

				if (sipCall->squ_event_mcast)
				{
					//Es el primer paquete que se recibe despu�s de que el primer squelch del grupo se ha activado
					//Se inicializa el n�mero de secuencia de los paquetes del grupo que se env�a por multicast
					sipCall->squ_event_mcast = PJ_FALSE;
					SipAgent::_FrecDesp->Set_mcast_seq(sipCall->_Index_group, seq);
				}

				if (sipCall->squ_status == SQU_ON)
				{
					if (sipCall->_Pertenece_a_grupo_FD_con_calculo_retardo_requerido)
					{
						//Esta sesion pertenece a un grupo FD que requiere calculo de retardo. En este caso 
						//se hace calculo de qidx

						pj_bool_t in_window = SipAgent::_FrecDesp->GetInWindow(sipCall->_Index_group, sipCall->_Index_sess);

						pj_uint32_t porcRSSI = (pj_uint32_t) sipCall->_Info.porcentajeRSSI;
						pj_uint32_t centralized_qidx_value = 0;  //Valor centralizado, calculado internamente, del qidx
						pj_uint32_t external_qidx_value = 0;	 //Valor de qidx recibido por rtp
						pj_uint32_t final_qidx_value = 0;		 //Valor del qidx resultante

						pj_uint8_t qidx_value_rtp = 0;
						pj_uint8_t qidx_method_rtp = 0;
						pj_bool_t qidx_received_by_rtp = PJ_FALSE;
						if (x && type == 0x1 && length == 1)
						{
							//Si recibimos el QIDX por el rtp lo tomamos.

							qidx_value_rtp = (pj_uint8_t) PJMEDIA_RTP_RD_EX_GET_VALUE(rtp_ext_info);

							qidx_method_rtp = qidx_value_rtp;
							qidx_method_rtp &= 0x7;
							qidx_value_rtp >>= 3;
							qidx_value_rtp &= 0x1F;
							qidx_received_by_rtp = PJ_TRUE;		
						}

						pj_bool_t calculado_internamente = PJ_FALSE;
						pj_bool_t recibido_externamente = PJ_FALSE;
						if (qidx_received_by_rtp && (sipCall->bss_method_type == RSSI_NUC) && (qidx_method_rtp == NUC_METHOD))
						{
							//Hemos recibido el qidx por el metodo nucleo. Eso es porque se lo hemos solicitado asi
							//a la pasarela porque no estamos en modo centralizado
							if (in_window || SipAgent::Coresip_Local_Config._Debug_BSS)
							{
								external_qidx_value = (pj_uint32_t) qidx_value_rtp;		//Esta en escala 0-31.	
								recibido_externamente = PJ_TRUE;
							}
						}
						else if (qidx_received_by_rtp && (sipCall->bss_method_type == RSSI) && (qidx_method_rtp == RSSI_METHOD))
						{
							//Hemos recibido el qidx de la radio y asi se lo habiamos solicitado, y el metodo no es centralizado
							if (in_window || SipAgent::Coresip_Local_Config._Debug_BSS)
							{
								//En este caso donde el peso del qidx centralizado (interno) es mayor que el minimo del de RSSI, hay que tener en cuenta el valor externo recibido por rtp del qidx
								external_qidx_value = (pj_uint32_t) qidx_value_rtp;		//Esta en escala 0-15.	
								external_qidx_value = (external_qidx_value * MAX_QIDX_ESCALE) / MAX_RSSI_ESCALE;  //Escala de 0-31
								recibido_externamente = PJ_TRUE;
							}
						}
						else if (sipCall->bss_method_type == CENTRAL)
						{
							if (qidx_received_by_rtp && (qidx_method_rtp == RSSI_METHOD))
							{
								//Hemos recibido el qidx de la radio y asi se lo habiamos solicitado, y el metodo es centralizado
								//Con lo cual, cuenta el rssi recibido de la radio.
								if (in_window || SipAgent::Coresip_Local_Config._Debug_BSS)
								{
									//En este caso donde el peso del qidx centralizado (interno) es mayor que el minimo del de RSSI, hay que tener en cuenta el valor externo recibido por rtp del qidx
									external_qidx_value = (pj_uint32_t) qidx_value_rtp;		//Esta en escala 0-15.	
									external_qidx_value = (external_qidx_value * MAX_QIDX_ESCALE) / MAX_RSSI_ESCALE;  //Escala de 0-31
									recibido_externamente = PJ_TRUE;
								}
							}

							if (porcRSSI < SipCall::MAX_porcentajeRSSI)
							{
								//En este caso donde el peso del qidx centralizado (interno) es menor que el maximo del de RSSI, hay que calcular internamente el qidx.
								if (in_window || SipAgent::Coresip_Local_Config._Debug_BSS)
								{						
									//Si estamos en la ventana de decision calculamos el qidx. 

									//Se procesa el paquete de voz para el calculo del Qidx
									pj_int16_t *ibuf = (pj_int16_t *) buf;
									for (unsigned int i = 0; i < frame_out.size/2; i++)
									{						
										sipCall->fPdataQidx[i] = (float) (ibuf[i]);
									}

									iir(sipCall->fPdataQidx, sipCall->afMuestrasIfRx, sipCall->b_dc, sipCall->a_dc, &sipCall->fFiltroDC_IfRx_ciX, &sipCall->fFiltroDC_IfRx_ciY, 1, frame_out.size/2);	//Filtro para eliminar la continua							
							
									process(&sipCall->PdataQidx, sipCall->afMuestrasIfRx, frame_out.size/2);

									centralized_qidx_value = (pj_uint32_t) quality_indicator(&sipCall->PdataQidx);				//Escala 0-50				
									centralized_qidx_value = (centralized_qidx_value * MAX_QIDX_ESCALE) / MAX_CENTRAL_ESCALE;		//Lo transformamos a escala 0- 15 (RSSI)																
									calculado_internamente = PJ_TRUE;
								
								}							
							}	
						}

						if (calculado_internamente && recibido_externamente)
						{
							final_qidx_value = ((external_qidx_value * porcRSSI) / MAX_porcentajeRSSI) + 
								((centralized_qidx_value * (MAX_porcentajeRSSI - porcRSSI)) / MAX_porcentajeRSSI);
						}
						else if (calculado_internamente)
						{
							final_qidx_value = centralized_qidx_value;		
						}
						else
						{
							final_qidx_value = external_qidx_value;			
						}

						if (in_window)
						{
							//Solo si estoy en la ventana de decision me guardo los valores del qidx 
							//en el array de donde se sacan los valores en la evaluacion de la mejor
							pj_mutex_lock(sipCall->bss_rx_mutex);
							if (sipCall->index_bss_rx_w < (MAX_BSS_SQU-1))
							{
								sipCall->bss_rx[sipCall->index_bss_rx_w] = (int) final_qidx_value;
								sipCall->index_bss_rx_w++;
							}
							pj_mutex_unlock(sipCall->bss_rx_mutex);
						}

						if (in_window || SipAgent::Coresip_Local_Config._Debug_BSS)
						{

							if (sipCall->last_qidx_value != (pj_uint8_t) final_qidx_value)
							{							
								sipCall->last_qidx_value = (pj_uint8_t) final_qidx_value; 

								PJ_LOG(3,(__FILE__, "BSS: %s QIDX final = %u, centralizado %u, externo %u", sipCall->DstUri, final_qidx_value, centralized_qidx_value, external_qidx_value));
							}

							SipAgent::_FrecDesp->SetBss(sipCall->_Index_group, sipCall->_Index_sess, 0, (pj_uint8_t) final_qidx_value);
																			//Lo guardamos en la sesion del grupo para luego enviarlo en el rdinfochanged
						}
					}

					pj_mutex_lock(sipCall->circ_buff_mutex);

					if (sipCall->squ_event)
					{
						//Se a�aden tantos silencios como valor de Retardo
						pjmedia_circ_buf_reset(sipCall->p_retbuff);
						while (pj_sem_trywait(sipCall->sem_out_circbuff) == PJ_SUCCESS);
						sipCall->wait_sem_out_circbuff = PJ_TRUE;
						for (pj_uint32_t i = 0; i < sipCall->Retardo; i++)
						{	
							pjmedia_circ_buf_write(sipCall->p_retbuff, (pj_int16_t *) sipCall->zerobuf, 1);
						}						
					}

					//Se a�ade al buffer circular el frame													
						
					pjmedia_circ_buf_write(sipCall->p_retbuff, (pj_int16_t *) buf, frame_out.size/2);

					pj_mutex_unlock(sipCall->circ_buff_mutex);

					pj_sem_post(sipCall->sem_out_circbuff);				
				}

				sipCall->squ_event = PJ_FALSE;
			}
			else if ((st == PJ_SUCCESS) && (frame_out.size != (SAMPLES_PER_FRAME/2) * sizeof(pj_int16_t)))
			{
				PJ_LOG(5,(__FILE__, "TRAZA: PAquete diferente de 80\n"));
			}
		}
		else
		{		
			buf[0] = RESTART_JBUF;
			size = 1;
		}

	}
}

/**
* ext_type_length. Primera palabra de la extension de cabecera que contiene 'type' y 'length'
* p_rtp_ext_info. Puntero que apunta donde comienza la extension de cabecera
* rtp_ext_length. Longitud en palabras de 32 bits de la extension de cabecera
*/
void SipCall::OnRdInfoChanged(void * stream, void *ext_type_length, pj_uint32_t rtp_ext_info, const void * p_rtp_ext_info, pj_uint32_t rtp_ext_length)
{		
	void * session = pjmedia_stream_get_user_data((pjmedia_stream*)stream);
	void * call = pjmedia_session_get_user_data((pjmedia_session*)session);
	pjmedia_rtp_ext_hdr *extention_type_length = (pjmedia_rtp_ext_hdr *) ext_type_length;

	if (!call) return;

	SipCall * sipCall = (SipCall*)((pjsua_call*)call)->user_data;

	if (!sipCall) return;
	if (sipCall->_Info.Type != CORESIP_CALL_RD) return;
	//assert(sipCall->_Info.Type == CORESIP_CALL_RD);

	CORESIP_RdInfo info = { CORESIP_PTT_OFF };

	info.PttType = (CORESIP_PttType)PJMEDIA_RTP_RD_EX_GET_PTT_TYPE(rtp_ext_info);
	info.PttId = (unsigned short)PJMEDIA_RTP_RD_EX_GET_PTT_ID(rtp_ext_info);
	info.PttMute = (int) PJMEDIA_RTP_RD_EX_GET_PM(rtp_ext_info);
	info.Squelch = PJMEDIA_RTP_RD_EX_GET_SQU(rtp_ext_info);
	info.Sct = PJMEDIA_RTP_RD_EX_GET_SCT(rtp_ext_info);
	
	//PJ_LOG(5,(__FILE__, "BSS: SQU info.Squelch %d FR %s %s", info.Squelch, sipCall->_RdFr, sipCall->DstUri));

	pj_bool_t ext_type_WG67;
	if (extention_type_length != NULL)
	{
		if ((extention_type_length->profile_data & 0xFF00) == 0x6700)
		{
			//La extension del cabecera es del tipo ED137 EUROCAE WG67
			ext_type_WG67 = PJ_TRUE;
		}
		else
		{
			ext_type_WG67 = PJ_FALSE;
		}
	}
	else
	{
		ext_type_WG67 = PJ_FALSE;
	}

	int x = PJMEDIA_RTP_RD_EX_GET_X(rtp_ext_info);
	pj_uint8_t *pextinfo = (pj_uint8_t *) p_rtp_ext_info;
	pextinfo += 2;		//Se pone el puntero donde comienzan las caracteristicas adicionales
	pj_int32_t nbytes = (pj_int32_t) (rtp_ext_length * 4);
	nbytes -= 2;

	while (ext_type_WG67 && x && nbytes > 0)
	{
		//Se tratan los TLVs	
		pj_uint32_t type, length;
		type = *pextinfo >> 4;
		type &= 0xF;
		length = *pextinfo;
		length &= 0xF;

		if (rtp_ext_length == 1 && length > 1)
			PJ_LOG(3,(__FILE__, "WARNING: Recibida extension de cabecera RTP erronea. rtp_ext_length %d type %d length %d", rtp_ext_length, type, length));

		pextinfo++;
		if (--nbytes == 0) break;

		if (type == 0x4 && length == 12 && sipCall->_Info.cld_supervision_time != 0)
		{
			//MAM recibido
			//Si cld_supervision_time es 0, entonces ignoramos el MAM. No queremos supervision de CLD

			PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay  radio uri %s", sipCall->DstUri));
			pj_bool_t request_MAM = PJ_FALSE;
			int ret = SipAgent::_FrecDesp->SetTimeDelay((pjmedia_stream*) stream, info.PttType, sipCall->_Index_group, sipCall->_Index_sess, pextinfo, &request_MAM);

			pj_time_val	delay;
			sipCall->Check_CLD_timer.id = Check_CLD_timer_IDLE;
			pjsua_cancel_timer(&sipCall->Check_CLD_timer);

			if ((ret < 0) && request_MAM)
			{
				delay.sec = 0;
				delay.msec = 3;
				pj_timer_entry_init( &sipCall->Check_CLD_timer, Check_CLD_timer_SEND_RMM, (void *) sipCall, Check_CLD_timer_cb);
			}
			else if (ret < 0)
			{
				PJ_LOG(3,(__FILE__, "ERROR: Time Delay cannot be assigned to session %p\n", session));
				//Se reintentará mandando un nuevo rmm despues del periodo de supervisión del cld				
				delay.sec = (long) sipCall->_Info.cld_supervision_time;
				delay.msec = 0;
				pj_timer_entry_init( &sipCall->Check_CLD_timer, Check_CLD_timer_SEND_RMM, (void *) sipCall, Check_CLD_timer_cb);
			}			
			else 
			{
				//Forzamos a enviar el CLD cuanto antes
				delay.sec = 0;
				delay.msec = 3;
				pj_timer_entry_init( &sipCall->Check_CLD_timer, Check_CLD_timer_SEND_CLD, (void *) sipCall, Check_CLD_timer_cb);
			}		
			pj_status_t st = pjsua_schedule_timer(&sipCall->Check_CLD_timer, &delay);
			if (st != PJ_SUCCESS)
			{
				sipCall->Check_CLD_timer.id = Check_CLD_timer_IDLE;
				PJ_CHECK_STATUS(st, ("ERROR en Check_CLD_timer"));
			}
			
		}

		pextinfo += length;
		nbytes -= length;

		if (nbytes < 0)
		{
			PJ_LOG(5,(__FILE__, "WARNING: Recibida extension de cabecera RTP erronea. rtp_ext_length %d type %d length %d", rtp_ext_length, type, length));
		}
	}

	pj_bool_t rdinfo_changed = PJ_FALSE;
	pj_uint8_t qidx;
	pj_uint8_t qidx_ml;
	pj_uint32_t Tred = 0;
	pj_uint32_t Ret = 0;
	SipAgent::_FrecDesp->GetQidx(sipCall->_Index_group, sipCall->_Index_sess, &qidx, &qidx_ml, &Tred);
	info.tx_owd = ((int) Tred * (int) 125) / 1000; //Se pasa a unidades de 1ms

	if (info.PttId != sipCall->RdInfo_prev.PttId)
	{
		//Actualizo aqui el Ptt id, y ya lo tiene el objeto sipcall para la seleccion de la mejor por BSS
		sipCall->RdInfo_prev.PttId = info.PttId;
		rdinfo_changed = PJ_TRUE;
	}

#if 0
#ifdef _DEBUG
	if (strcmp(sipCall->DstUri, "<sip:RX-01@192.168.1.18:5060>") == 0) qidx = 15;
	if (strcmp(sipCall->DstUri, "<sip:RX-02@192.168.1.18:5060>") == 0) qidx = 14;
	if (strcmp(sipCall->DstUri, "<sip:RX-03@192.168.1.18:5060>") == 0) qidx = 13;
	if (strcmp(sipCall->DstUri, "<sip:RX-04@192.168.1.18:5060>") == 0) qidx = 12;
#endif
#endif

	if ((sipCall->squ_status != info.Squelch) && (sipCall->Wait_init_timer.id == 0))
	{
		pj_ssize_t size;

		size = (pj_ssize_t)((SAMPLES_PER_FRAME/2) * sizeof(pj_int16_t));

		//Ha cambiado el estado del squelch
		if (info.Squelch == 1)
		{		
			//SQU ON

			int nsqus = SipAgent::_FrecDesp->SetSquSt(sipCall->_Index_group, sipCall->_Index_sess, PJ_TRUE, NULL);
			Ret = SipAgent::_FrecDesp->GetRetToApply(sipCall->_Index_group, sipCall->_Index_sess);

			PJ_LOG(5,(__FILE__, "BSS: SQU ON FR %s %s Ret %d ms GROUP %d SESS %d", sipCall->_RdFr, sipCall->DstUri, (Ret*125)/1000, sipCall->_Index_group, sipCall->_Index_sess));

			int nsessiones_tx_only = 0;
			int nsessiones = SipAgent::_FrecDesp->GetSessionsCountInGroup(sipCall->_Index_group, NULL, &nsessiones_tx_only);
			nsessiones -= nsessiones_tx_only;
			if (nsqus == 1)
			{
				//Este es el unico squelch activado
				SipAgent::_FrecDesp->SetSelected(sipCall, PJ_TRUE, PJ_FALSE);						
						
				//Se para el timer de la ventana de decision por si estuviera arrancado
				if (sipCall->window_timer.id == 1)
				{
					SipAgent::_FrecDesp->SetInWindow(sipCall->_Index_group, PJ_FALSE);
				}
				sipCall->window_timer.id = 0;
				pjsua_cancel_timer(&sipCall->window_timer);

				SipAgent::_FrecDesp->SetSelectedUri(sipCall);   //Es el primer squelch que llega. Asignamos esta uri como seleccionada
				if (nsessiones > 1)
				{
					//Hay grupo bss porque tiene m�s de una sesi�n, sin contar los tx only
					if (sipCall->_Info.AudioInBssWindow)
					{
						//Se activa el envio por multicast de esta sesion
						SipAgent::_FrecDesp->EnableMulticast(sipCall, PJ_TRUE, PJ_FALSE);	
						if (SipAgent::Coresip_Local_Config._Debug_BSS)
							PJ_LOG(3,(__FILE__, "BSS: SQU ON FR %s %s SELECCIONADO", sipCall->_RdFr, sipCall->DstUri));
					}
					else
					{
						//No hay audio durante la ventana de decisi�n
						SipAgent::_FrecDesp->EnableMulticast(sipCall, PJ_FALSE, PJ_TRUE);
					}
				}
				else
				{
					//Solo hay un miembro en el grupo
					//No hay grupo bss. Se activa el env�o por multicast
					SipAgent::_FrecDesp->EnableMulticast(sipCall, PJ_TRUE, PJ_FALSE);
					if (SipAgent::Coresip_Local_Config._Debug_BSS)
						PJ_LOG(3,(__FILE__, "BSS: SQU ON FR %s %s SELECCIONADO", sipCall->_RdFr, sipCall->DstUri));
				}						

				sipCall->squ_event_mcast = PJ_TRUE;		//Se activa evento con el primer squelch de un grupo

				SipAgent::_FrecDesp->Set_group_multicast_socket(sipCall->_Index_group, &sipCall->_RdSendTo);

				if (nsessiones > 1)
				{
					//Se arranca el timer de la ventana de decision. En caso de que haya mas de una session en
					//el grupo
					pj_time_val	delay;
					delay.sec = ((long) sipCall->_Info.BssWindows) / 1000;
					delay.msec = ((long) sipCall->_Info.BssWindows) - (delay.sec * 1000);	
					pj_timer_entry_init( &sipCall->window_timer, 1, (void *) sipCall, window_timer_cb);
					pj_status_t st = pjsua_schedule_timer(&sipCall->window_timer, &delay);
					if (st != PJ_SUCCESS)
					{
						sipCall->window_timer.id = 0;
						PJ_CHECK_STATUS(st, ("ERROR en window_timer"));
					}
					else
					{
						//Al resto del grupo se activa que estamos en la ventana de decision
						SipAgent::_FrecDesp->SetInWindow(sipCall->_Index_group, PJ_TRUE);
					}
				}
			}

			//Se inicializa el indice de escritura en el array que almacena los valores de bss recibidos.
			pj_mutex_lock(sipCall->bss_rx_mutex);
			sipCall->index_bss_rx_w = 0;	
			pj_mutex_unlock(sipCall->bss_rx_mutex);

			if(!sipCall->_Info.AudioSync)
			{
				//Se fuerza que el retardo a aplicar en la recepcion de audio sea 0
				Ret = 0;
			}
										
			if (Ret < 80)		//Si el retardo es menor de 10ms=80*125us no se aplica retardo
			{
				sipCall->hay_retardo = PJ_FALSE;
				sipCall->Retardo = 0;
				sipCall->squoff_event_mcast = PJ_FALSE;
			}
			else 
			{
				sipCall->hay_retardo = PJ_TRUE;
				sipCall->Retardo = Ret;
			}
			sipCall->squ_event = PJ_TRUE;   //El estado a cambiado a squ activo. Activamos el evento de squelch				
			sipCall->primer_paquete_despues_squelch = PJ_TRUE;
		}
		else
		{	
			//SQU OFF
			PJ_LOG(5,(__FILE__, "BSS: SQU OFF FR %s %s", sipCall->_RdFr, sipCall->DstUri));			

			int sq_air_count = 0;	//Contador de squelches de avion activados en el grupo
			int nsqus = SipAgent::_FrecDesp->SetSquSt(sipCall->_Index_group, sipCall->_Index_sess, PJ_FALSE, &sq_air_count);
			if (nsqus < 1)
			{
				//No hay ningun squelch del grupo activado
				//Se para el timer de la ventana de decision por si estaba activado
				if (sipCall->window_timer.id == 1)
				{
					SipAgent::_FrecDesp->SetInWindow(sipCall->_Index_group, PJ_FALSE);
				}
				sipCall->window_timer.id = 0;
				pjsua_cancel_timer(&sipCall->window_timer);
				SipAgent::_FrecDesp->ClrSelectedUri(sipCall);
			}
			else if (sipCall->window_timer.id == 1)  //Estamos en la ventana de decision
			{
				//Hay otros squelch activados
				if (sipCall->_Sending_Multicast_enabled)
				{
					//Esta sesion es la que estaba activada. Cambiamos a otra que tenga squelch activado
					//Por ejemplo la que tenga mejor bss.
					SipAgent::_FrecDesp->SetBetterSession(sipCall, FrecDesp::IN_WINDOW, !FrecDesp::ONLY_SELECTED_IN_WINDOW);
				}
			}
			else if (sipCall->_Sending_Multicast_enabled)
			{
				//Estamos fuera de la ventana de decision y se detecta squ off en la sesion que ahora es la activa

				unsigned short selectedUriPttId = 0;
				SipAgent::_FrecDesp->GetSelectedUri(sipCall, NULL, &selectedUriPttId);
				if (selectedUriPttId != 0)
				{
					//Si esta rama seleccionada no era procedente de avion 					
					//Forzamos un estado de ningun squelch activado en el grupo para forzar el arranque de una nueva ventana de decision					

					SipAgent::_FrecDesp->EnableMulticast(sipCall, PJ_FALSE, PJ_TRUE);					
					SipAgent::_FrecDesp->ClrSelectedUri(sipCall);
					SipAgent::_FrecDesp->ClrAllSquSt(sipCall->_Index_group);

					//La 'selected' del grupo no la pongo a cero porque la aplicacion del Nodebox la necesita
					//para quitar la señalizacion del Squelch. 
					//SipAgent::_FrecDesp->SetSelected(sipCall, PJ_FALSE, PJ_TRUE);
				}
				else if (selectedUriPttId == 0 && sq_air_count == 0)
				{
					//Por el contrario
					//Si el squelch de esta rama era procedente de avion pero ya solamente quedan squelches activados 
					//que no son de avion, entonces se supone que lo que transmitimos tiene que recibirse por todas las ramas 
					//Por tanto no hay que hacer recalificacion y seleccionar otra rama y por tanto en este caso
					//hay que hacer lo mismo que cuando se desactiva cualquier squelch. Es decir
					//Solo se puede activar la sesión que se activo durante la ventana de decision
					//O sea que se hace lo mismo que en el siguiente else
					//Lo hacemos asi para que quede documentado.
					SipAgent::_FrecDesp->SetBetterSession(sipCall, !FrecDesp::IN_WINDOW, FrecDesp::ONLY_SELECTED_IN_WINDOW);
				}
				else
				{
					//Solo se puede activar la sesión que se activo durante la ventana de decision
					SipAgent::_FrecDesp->SetBetterSession(sipCall, !FrecDesp::IN_WINDOW, FrecDesp::ONLY_SELECTED_IN_WINDOW);
				}
			}

			sipCall->squoff_event_mcast = PJ_TRUE;
			pj_sem_post(sipCall->sem_out_circbuff);		//Al salir del wait de este semaforo con el flag squoff_event_mcast
														//nos indica que fin de squelch y no esperar con el semaforo sino con el sleep
			//Al desactivarse  el squelch se ponen silencios en buffer circular
			pj_mutex_lock(sipCall->circ_buff_mutex);
			for (pj_uint32_t i = 0; i < 6; i++)
			{						
				pjmedia_circ_buf_write(sipCall->p_retbuff, (pj_int16_t *) sipCall->zerobuf, size/2);
			}						
			pj_mutex_unlock(sipCall->circ_buff_mutex);
					
		}		

		sipCall->squ_status = info.Squelch;
	}	

	pjmedia_session_info sess_info;
	pjmedia_session_get_info((pjmedia_session*)session, &sess_info);					

	pjsua_call * psuacall = (pjsua_call *) call;
	pjmedia_transport_info tpinfo;
	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(psuacall->med_tp, &tpinfo);

	info.rx_rtp_port = (int) pj_ntohs(sess_info.stream_info[0].rem_addr.ipv4.sin_port);
	info.rx_qidx = qidx;
	info.rx_selected = SipAgent::_FrecDesp->IsBssSelected(sipCall);
	info.tx_rtp_port = (int) pj_ntohs(tpinfo.sock_info.rtp_addr_name.ipv4.sin_port);
	SipAgent::_FrecDesp->GetLastCld(sipCall->_Index_group, sipCall->_Index_sess, (pj_uint8_t *) &info.tx_cld);
	
	info.tx_cld &= ~0x80;
	info.tx_cld *= 2;		//Se multiplica por 2 para pasar a unidades de 1 ms

	//info.tx_cld = 0;
	//info.tx_owd = 0;
	
	pj_mutex_lock(sipCall->RdInfo_prev_mutex);
	if (memcmp(&info, &sipCall->RdInfo_prev, sizeof(CORESIP_RdInfo)) != 0)
		rdinfo_changed = PJ_TRUE;
	pj_mutex_unlock(sipCall->RdInfo_prev_mutex);

	if (rdinfo_changed)
	{
		//Llamo a la callback si cambia algun valor en la info

		PJ_LOG(5,(__FILE__, "onRdinfochanged: ########## dst %s PttType %d PttId %d rx_selected %d Squelch %d", sipCall->DstUri, info.PttType, info.PttId, info.rx_selected, info.Squelch));
		PJ_LOG(5,(__FILE__, "onRdinfochanged: PttMute %d Sct %d rx_rtp_port %d rx_qidx %d", info.PttMute, info.Sct, info.rx_rtp_port, info.rx_qidx));
		PJ_LOG(5,(__FILE__, "onRdinfochanged: tx_rtp_port %d tx_cld %d tx_owd %d sipCall->Ptt_off_timer.id %d ############", info.tx_rtp_port, info.tx_cld, info.tx_owd, sipCall->Ptt_off_timer.id));

		if (sipCall->Ptt_off_timer.id == 0)
		{
			if (info.Squelch == 0) info.rx_qidx = 0;	//Si no hay squelch el qidx que se envia a la aplicacion es cero

			//Actualizamos los parametros que no se toman en la callback OnRdInfochanged 
			//Esto hay que hacerlo siempre que se llame a RdInfoCb
			//-->
			info.rx_selected = SipAgent::_FrecDesp->IsBssSelected(sipCall);
			//<--

			if (SipAgent::Cb.RdInfoCb)
			{
				PJ_LOG(5,(__FILE__, "onRdinfochanged: envia a nodebox. dst %s PttType %d PttId %d rx_selected %d Squelch %d", sipCall->DstUri, info.PttType, info.PttId, info.rx_selected, info.Squelch));
				SipAgent::Cb.RdInfoCb(((pjsua_call*)call)->index | CORESIP_CALL_ID, &info);
			}
		}

		pj_mutex_lock(sipCall->RdInfo_prev_mutex);
		memcpy(&sipCall->RdInfo_prev, &info, sizeof(CORESIP_RdInfo));			
		pj_mutex_unlock(sipCall->RdInfo_prev_mutex);
	}
}

/**
 * window_timer_cb.	...
 * Callback que se llama cuando window_timer expira. Termina la ventana de decision
 * @return	
 */
void SipCall::window_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
	SipCall *wp = (SipCall *) te->user_data;

    PJ_UNUSED_ARG(th);

	PJ_LOG(5,(__FILE__, "BSS: VENCE VENTANA FR %s", wp->_RdFr));

	SipAgent::_FrecDesp->SetInWindow(wp->_Index_group, PJ_FALSE);

	if (wp->window_timer.id == 1)
	{
		wp->window_timer.id = 0;
		pjsua_cancel_timer(&wp->window_timer);

		SipAgent::_FrecDesp->SetBetterSession(wp, FrecDesp::IN_WINDOW, !FrecDesp::ONLY_SELECTED_IN_WINDOW);
	}
	else
	{
		wp->window_timer.id = 0;
		pjsua_cancel_timer(&wp->window_timer);
	}
}

int SipCall::Out_circbuff_Th(void *proc)
{
	SipCall *wp = (SipCall *)proc;
	pj_status_t st = PJ_SUCCESS;
	pj_bool_t primer_paquete = PJ_TRUE;

	pj_thread_desc desc;
    pj_thread_t *this_thread;
	pj_status_t rc;

	pj_bzero(desc, sizeof(desc));

    rc = pj_thread_register("Out_circbuff_Th", desc, &this_thread);
    if (rc != PJ_SUCCESS) {
		PJ_LOG(3,(__FILE__, "...error in pj_thread_register Out_circbuff_Th!"));
        return 0;
    }

    /* Test that pj_thread_this() works */
    this_thread = pj_thread_this();
    if (this_thread == NULL) {
        PJ_LOG(3,(__FILE__, "...error: Out_circbuff_Th pj_thread_this() returns NULL!"));
        return 0;
    }

    /* Test that pj_thread_get_name() works */
    if (pj_thread_get_name(this_thread) == NULL) {
        PJ_LOG(3,(__FILE__, "...error: Out_circbuff_Th pj_thread_get_name() returns NULL!"));
        return 0;
    }				
	
	while (wp->out_circbuff_thread_run)
	{
		if (wp->wait_sem_out_circbuff) st = pj_sem_wait(wp->sem_out_circbuff);
		else pj_thread_sleep(PTIME/2);
		if (!wp->out_circbuff_thread_run) break;
		if (wp->squoff_event_mcast) 
		{
			wp->squoff_event_mcast = PJ_FALSE;
			wp->wait_sem_out_circbuff = PJ_FALSE;
			continue;
		}	

		pj_bool_t packet_present = PJ_FALSE;
		pj_ssize_t size_packet = (pj_ssize_t)((SAMPLES_PER_FRAME/2) * sizeof(pj_int16_t));   //en bytes
		pj_ssize_t size_packet_x = size_packet + sizeof(unsigned);
		char buf_out[640];
	
		pj_mutex_lock(wp->circ_buff_mutex);	
		unsigned int cbuf_len = pjmedia_circ_buf_get_len(wp->p_retbuff);
		if (cbuf_len < ((unsigned int) (size_packet/2))) 
		{				
			packet_present = PJ_FALSE;

			pjmedia_circ_buf_reset(wp->p_retbuff);
			pj_mutex_unlock(wp->circ_buff_mutex);
			wp->wait_sem_out_circbuff = PJ_TRUE;
			
			if (SipAgent::_FrecDesp->IsBssSelected(wp) && wp->window_timer.id == 0)
			{
				//Este es el seleccionado en el bss y no se est� en la ventana de selecci�n
				//Desaparece la seleccion de todos. 
				//SipAgent::_FrecDesp->EnableMulticast(wp, PJ_FALSE, PJ_TRUE);
				//SipAgent::_FrecDesp->SetSelected(wp, PJ_FALSE, PJ_TRUE);

				//Mandamos el dato para resetear el buffer jitter de recepci�n de audio en el puesto
				pj_sockaddr_in *RdsndTo; 
				SipAgent::_FrecDesp->Get_group_multicast_socket(wp->_Index_group, &RdsndTo);
				buf_out[0] = RESTART_JBUF;
				size_packet = 1;
				pj_sock_sendto(wp->_RdSendSock, buf_out, &size_packet, 0, RdsndTo, sizeof(pj_sockaddr_in));
			}

			continue;
		}
		else
		{
			packet_present = PJ_TRUE;
		}	

		if (packet_present == PJ_FALSE)
		{			
			if (wp->squ_status == SQU_ON)
			{
				PJ_LOG(5,(__FILE__, "TRAZA: Multicast_clock_cb SIN MUESTRAS"));
			}			
		}
		else
		{
#if 0
			if ((wp->Retardo > 0) && wp->wait_sem_out_circbuff)
			{
				pj_uint32_t umbral_l, umbral_h;
				if (wp->Retardo > size_packet/2) 
				{
					umbral_l = wp->Retardo - size_packet/2;
					umbral_h = wp->Retardo + size_packet/2;
				}
				else 
				{
					umbral_l = cbuf_len;
					umbral_h = cbuf_len;
				}

				if (cbuf_len < umbral_l) 
				{
					PJ_LOG(5,(__FILE__, "TRAZA: RETARDO MENOR antes cbuf_len %u Retardo %u\n", cbuf_len, wp->Retardo));
					//pjmedia_circ_buf_write(wp->p_retbuff, (pj_int16_t *) wp->zerobuf, size_packet/2);
					while (cbuf_len < wp->Retardo)
					{
						pjmedia_circ_buf_write(wp->p_retbuff, (pj_int16_t *) wp->zerobuf, 1);
						cbuf_len = pjmedia_circ_buf_get_len(wp->p_retbuff);
					}
					//cbuf_len = pjmedia_circ_buf_get_len(wp->p_retbuff);
					PJ_LOG(5,(__FILE__, "TRAZA: RETARDO MENOR despues cbuf_len %u Retardo %u\n", cbuf_len, wp->Retardo));
				}
				else if (cbuf_len > umbral_h) 
				{
					PJ_LOG(5,(__FILE__, "TRAZA: RETARDO MAYOR antes cbuf_len %u Retardo %u\n", cbuf_len, wp->Retardo));
					//pjmedia_circ_buf_read(wp->p_retbuff, (pj_int16_t *) wp->zerobuf, size_packet/2);
					while (cbuf_len > umbral_h)
					{
						pj_int16_t nullc;
						pjmedia_circ_buf_read(wp->p_retbuff, (pj_int16_t *) &nullc, 1);
						cbuf_len = pjmedia_circ_buf_get_len(wp->p_retbuff);
					}
					//cbuf_len = pjmedia_circ_buf_get_len(wp->p_retbuff);
					PJ_LOG(5,(__FILE__, "TRAZA: RETARDO MAYOR despues cbuf_len %u Retardo %u\n", cbuf_len, wp->Retardo));
				}
				else ;
			}
#endif


			pjmedia_circ_buf_read(wp->p_retbuff, (pj_int16_t *) buf_out, size_packet/2);
			pj_mutex_unlock(wp->circ_buff_mutex);				
					
			if (wp->_Sending_Multicast_enabled)
			{
				unsigned nseq = SipAgent::_FrecDesp->Get_mcast_seq(wp->_Index_group);				

				*((unsigned*)((char*)buf_out + size_packet)) = pj_htonl(nseq);
				pj_sockaddr_in *RdsndTo; 
				SipAgent::_FrecDesp->Get_group_multicast_socket(wp->_Index_group, &RdsndTo);

				if (RdsndTo == NULL) 
				{
					RdsndTo = &wp->_RdSendTo;		//Si se ha perdido la direccion usamos la propia
					SipAgent::_FrecDesp->Set_group_multicast_socket(wp->_Index_group, RdsndTo);
				}

				if (primer_paquete)
				{					
					char data = RESTART_JBUF;
					pj_ssize_t siz = 1;
					pj_sock_sendto(wp->_RdSendSock, &data, &siz, 0, RdsndTo, sizeof(pj_sockaddr_in));
					primer_paquete = PJ_FALSE;
				}

				pj_sock_sendto(wp->_RdSendSock, buf_out, &size_packet_x, 0, RdsndTo, sizeof(pj_sockaddr_in));
				//pj_sock_sendto(wp->_RdSendSock, buf_out, &size_packet_x, 0, &wp->_RdSendTo, sizeof(pj_sockaddr_in));
			}
			continue;
		}

		pj_mutex_unlock(wp->circ_buff_mutex);

		continue;
	}

	return 0;
}

CORESIP_CallInfo *SipCall::GetCORESIP_CallInfo()
{
	return &_Info;
}

//Retorna el Bss teniendo en cuenta el retardo aplicado para la sincronizaci�n en la ventana de decision
int SipCall::GetSyncBss()
{
#if 0
#ifdef _DEBUG
	if (strcmp(DstUri, "<sip:RX-01@192.168.1.18:5060>") == 0) return 15;
	if (strcmp(DstUri, "<sip:RX-02@192.168.1.18:5060>") == 0) return 14;
	if (strcmp(DstUri, "<sip:RX-03@192.168.1.18:5060>") == 0) return 13;
	if (strcmp(DstUri, "<sip:RX-04@192.168.1.18:5060>") == 0) return 12;
#endif
#endif

	int bss = 0;
	if (Retardo == 0)
	{
		//No se ha aplicado retardo. Por tanto se retorna el ultimo bss recibido
		pj_mutex_lock(bss_rx_mutex);
		if (index_bss_rx_w > 0)
		{
			//Se ha recibido alg�n bss
			bss = bss_rx[index_bss_rx_w - 1];
		}
		pj_mutex_unlock(bss_rx_mutex);
		return bss;
	}

	//El indice del bss que buscamos, dentro del array bss_rx, es el ultimo menos el correspondiente al retardo.
	//El Retardo esta en unidades de 125us y el bss se almacena en el array cada vez que se ejecuta la funcion OnRdRtp
	//es decir, cada 10 ms
	int dif_index_bss = Retardo / (SAMPLES_PER_FRAME_RTP/2);	

	pj_mutex_lock(bss_rx_mutex);
	if ((index_bss_rx_w - 1) > dif_index_bss)
	{
		bss = bss_rx[(index_bss_rx_w - 1) - dif_index_bss];
	}
	else
	{
		bss = bss_rx[0];
	}
	pj_mutex_unlock(bss_rx_mutex);

	return bss;
}

/*Timer para solicitar periodicamente el MAM para el climax*/
void SipCall::Check_CLD_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
	SipCall *wp = (SipCall *)te->user_data;
	pj_bool_t calcular_retardos = PJ_FALSE;		//Indica si hay que calcular retardos para esta radio
	pjmedia_session *session = NULL;

	if (wp == NULL) return;
	//if (!wp->valid_sess) return;

	if ((wp->Check_CLD_timer.id == Check_CLD_timer_IDLE) || (!wp->valid_sess) || (wp->_Info.cld_supervision_time == 0))
	{
		//Si cld_supervision_time vale 0, no queremos supervision de CLD
		wp->Check_CLD_timer.id = Check_CLD_timer_IDLE;
		pjsua_cancel_timer(&wp->Check_CLD_timer);
		return;
	}
	
	pjsua_call_info callInfo;
	pj_status_t ret = pjsua_call_get_info(wp->_Id, &callInfo);	
	if (callInfo.state != PJSIP_INV_STATE_CONFIRMED) return;

	/*Se evalua si esta sesion del grupo necesita calculo de retardo*/

	if ((ret == PJ_SUCCESS) && wp->bss_method_type != NINGUNO)
	{
		session = pjsua_call_get_media_session(wp->_Id);
		int nsession_rx_only = 0;
		int nsession_tx_only = 0;
		int nsessions_in_group = SipAgent::_FrecDesp->GetSessionsCountInGroup(wp->_Index_group, &nsession_rx_only, &nsession_tx_only);

		if ((session != NULL) && (nsessions_in_group > 1)) 
		{				
			if (wp->_Info.Flags & CORESIP_CALL_RD_RXONLY)
			{
				if (nsession_rx_only > 1) calcular_retardos = PJ_TRUE;
				else if (nsessions_in_group != (nsession_rx_only + nsession_tx_only)) calcular_retardos = PJ_TRUE;
			}
			else if (wp->_Info.Flags & CORESIP_CALL_RD_TXONLY)
			{
				if (nsession_tx_only > 1) calcular_retardos = PJ_TRUE;
				else if (nsessions_in_group != (nsession_rx_only + nsession_tx_only)) calcular_retardos = PJ_TRUE;
			}
			else
			{
				calcular_retardos = PJ_TRUE;
			}
		}
	}

	if (ret == PJ_SUCCESS)
	{
		wp->_Pertenece_a_grupo_FD_con_calculo_retardo_requerido = calcular_retardos;
	}

	if ((ret == PJ_SUCCESS) && (calcular_retardos) && (wp->Check_CLD_timer.id == Check_CLD_timer_SEND_CLD))
	{
		pj_uint32_t rtp_ext_info = 0;
		pj_uint8_t cld = 0;
		
		if (SipAgent::_FrecDesp->GetCLD(wp->_Id, &cld) != -1)
		{		
			ret = pjsua_call_get_info(wp->_Id, &callInfo);	
			if (ret != PJ_SUCCESS) return;
			if (callInfo.state != PJSIP_INV_STATE_CONFIRMED) return;

			pjmedia_stream * stream = NULL;
			session = pjsua_call_get_media_session(wp->_Id);
			if (session != NULL)
			{
				stream = pjmedia_session_get_stream(session, 0);
			
				if (stream != NULL)
				{
					pjmedia_stream_get_rtp_ext_tx_info(stream, &rtp_ext_info);
					PJMEDIA_RTP_RD_EX_SET_X(rtp_ext_info, 1);			
					PJMEDIA_RTP_RD_EX_SET_TYPE(rtp_ext_info, 2);
					PJMEDIA_RTP_RD_EX_SET_LENGTH(rtp_ext_info, 1);
					PJMEDIA_RTP_RD_EX_SET_CLD(rtp_ext_info, (cld));
					pjmedia_stream_set_rtp_ext_tx_info(stream, rtp_ext_info);

					if (cld != 0) 
						PJ_LOG(5,(__FILE__, "CLIMAX: SipCall::Check_CLD_timer_cb GetCLD uri %s CLD DISTINTO CERO %d ms", wp->DstUri, cld*2));
					else
						PJ_LOG(5,(__FILE__, "CLIMAX: SipCall::Check_CLD_timer_cb GetCLD uri %s CLD %d ms", wp->DstUri, cld*2));
				}
			}
		}		
	}
	else if ((ret == PJ_SUCCESS) && (calcular_retardos) && (wp->Check_CLD_timer.id == Check_CLD_timer_SEND_RMM))
	{
		PJ_LOG(5,(__FILE__, "CLIMAX: SipCall::Check_CLD_timer_cb antes pjmedia_session_force_request_MAM uri %s", wp->DstUri));
		ret = pjsua_call_get_info(wp->_Id, &callInfo);	
		if (ret != PJ_SUCCESS) return;
		if (callInfo.state != PJSIP_INV_STATE_CONFIRMED) return;
		session = pjsua_call_get_media_session(wp->_Id);
		if (session != NULL)
			pjmedia_session_force_request_MAM(session);
	}
	
	wp->Check_CLD_timer.id = Check_CLD_timer_IDLE;
	pjsua_cancel_timer(&wp->Check_CLD_timer);

	pj_time_val	delay;	
	delay.sec = (long) wp->_Info.cld_supervision_time;	
	delay.msec = 0;
	pj_timer_entry_init( &wp->Check_CLD_timer, Check_CLD_timer_SEND_RMM, (void *) wp, Check_CLD_timer_cb);
	pj_status_t st = pjsua_schedule_timer(&wp->Check_CLD_timer, &delay);
	if (st != PJ_SUCCESS)
	{
		wp->Check_CLD_timer.id = Check_CLD_timer_IDLE;
		PJ_CHECK_STATUS(st, ("ERROR en Check_CLD_timer"));
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
			else if ((call->_Codecs & 2)!=2 && strncmp((char *)ar.pt.ptr, "0",1)==0)	// PCM �-law
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
				// A�adir atributo txrxmode
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
void SipCall::OnCreateSdp(pj_pool_t *pool, int call_id, void * local_sdp, void * incoming_rdata)
{
	if (pool != NULL && call_id != PJSUA_INVALID_ID && local_sdp != NULL)
	{
		pjmedia_sdp_session * sdp = (pjmedia_sdp_session*)local_sdp;		
		SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);
		pjsip_rx_data * rdata = (pjsip_rx_data*)incoming_rdata;
		pjsua_call_info callinfo;
		if (pjsua_call_get_info(call_id, &callinfo) != PJ_SUCCESS)
		{
			callinfo.id = PJSUA_INVALID_ID;
		}

		pj_bool_t rdAccount = PJ_FALSE;				//Indica si add_id es un account tipo radio GRS

		if (callinfo.id != PJSUA_INVALID_ID)
		{
			ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(callinfo.acc_id);
			if (extraParamAccCfg != NULL)
			{
				rdAccount = extraParamAccCfg->rdAccount;
			}
		}
		
		if ((SipAgent::_Radio_UA || rdAccount) && pool)
		{
			//Es un agente radio o el account es de una radio GRS

			pjmedia_sdp_session *rem_sdp = NULL;
			pj_bool_t sdp_bss = PJ_FALSE;

			if (rdata != NULL)
			{
				pj_status_t status = pjmedia_sdp_parse(pool, (char*)rdata->msg_info.msg->body->data, rdata->msg_info.msg->body->len, &rem_sdp);
				if (status == PJ_SUCCESS) 
				{
					/* Validate */
					status = pjmedia_sdp_validate(rem_sdp);
				}
			
				if (status == PJ_SUCCESS && rem_sdp->media_count > 0)
				{
					for (unsigned int i = 0; i < rem_sdp->media_count; i++)
					{				
						pjmedia_sdp_attr *a;

						if ((a=pjmedia_sdp_media_find_attr2(rem_sdp->media[i], "bss", NULL)) != NULL)
						{
							//El SDP incluye el campo bss, por tanto tenemos que enviar el qidx con el rtp
							sdp_bss = PJ_TRUE;
						}
					}
				}
			}

			//Es un agente radio. Inicializamos aqui el SDP para este caso.
			//Utilizamos el pool que se pasa como parametro.
			pjmedia_sdp_attr * a;
			char kap[15], kam[15];

			pj_str_t rtpmap = pj_str("123 R2S/8000");
			a = pjmedia_sdp_attr_create(pool, "rtpmap", &rtpmap);
			pjmedia_sdp_media_add_attr(sdp->media[0], a);

			pj_str_t typedata = pj_str("Radio");
			a = pjmedia_sdp_attr_create(pool, "type", &typedata);
			pjmedia_sdp_media_add_attr(sdp->media[0], a);

			SipAgent::KeepAliveParams(kap, kam);
			a = pjmedia_sdp_attr_create(pool, "R2S-KeepAlivePeriod", &(pj_str(kap)));
			pjmedia_sdp_media_add_attr(sdp->media[0], a);
			a = pjmedia_sdp_attr_create(pool, "R2S-KeepAliveMultiplier", &(pj_str(kam)));
			pjmedia_sdp_media_add_attr(sdp->media[0], a);

			pj_str_t rtphe = pj_str("1");
			a = pjmedia_sdp_attr_create(pool, "rtphe",&rtphe);
			pjmedia_sdp_media_add_attr(sdp->media[0], a);

			pj_str_t pttid = pj_str("1");
			a = pjmedia_sdp_attr_create(pool, "ptt-id",&pttid);
			pjmedia_sdp_media_add_attr(sdp->media[0], a);

			pj_str_t pttrep = pj_str("1");
			a = pjmedia_sdp_attr_create(pool, "ptt_rep",&pttrep);
			pjmedia_sdp_media_add_attr(sdp->media[0], a);

			if (sdp_bss)
			{
				pj_str_t bssmet = pj_str("RSSI");
				a = pjmedia_sdp_attr_create(pool, "bss",&bssmet);
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
			}
		}
		else if (call)
		{
			if (call->_Info.Type == CORESIP_CALL_RD)
			{
				//En este caso somos un agente de telefonia que abre sesion con una radio. Se inicializa aqui el SDP
				//SE usa el pool del objeto SipCall

				char kap[15], kam[15];
				pjmedia_sdp_attr * a;
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
				pj_str_t typedata = TestRadioIdle==true ? (pj_str("Radio-Idle")) :
					call->_Info.Flags & CORESIP_CALL_RD_COUPLING ? (pj_str("Coupling")) :
					/*call->_Info.Flags & CORESIP_CALL_RD_RXONLY ? (pj_str("Radio-Rxonly")) :*/
					call->_Info.Flags & CORESIP_CALL_RD_RXONLY ? (pj_str("Radio")) :
					call->_Info.Flags & CORESIP_CALL_RD_TXONLY ? (pj_str("Radio")) : (pj_str("Radio-TxRx"));

				a = pjmedia_sdp_attr_create(call->_Pool, "type", &typedata);
				pjmedia_sdp_media_add_attr(sdp->media[0], a);

				/** CODEC 123 */
				pj_str_t *fmt = &sdp->media[0]->desc.fmt[sdp->media[0]->desc.fmt_count++];
				fmt->ptr = (char*) pj_pool_alloc(call->_Pool, 8);
				fmt->slen = pj_utoa(123, fmt->ptr);

				a = pjmedia_sdp_attr_create(call->_Pool, "rtpmap", &(pj_str("123 R2S/8000")));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);

				pj_str_t txrxmodedata = (call->_Info.Flags & CORESIP_CALL_RD_RXONLY) ? (pj_str("Rx"))  : 
					(call->_Info.Flags & CORESIP_CALL_RD_TXONLY) ? (pj_str("Tx")) : (pj_str("TxRx"));
				a = pjmedia_sdp_attr_create(call->_Pool, "txrxmode", &txrxmodedata);
				pjmedia_sdp_media_add_attr(sdp->media[0], a);

				/*No envio el fid en el SDP.*/
				/*a = pjmedia_sdp_attr_create(call->_Pool, "fid", &(pj_str(call->_RdFr)));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
				*/

				a = pjmedia_sdp_attr_create(call->_Pool, "R2S-KeepAlivePeriod", &(pj_str(kap)));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
				a = pjmedia_sdp_attr_create(call->_Pool, "R2S-KeepAliveMultiplier", &(pj_str(kam)));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);

				if (1) 	
				{			
					if (!(call->_Info.Flags & CORESIP_CALL_RD_TXONLY) && (call->bss_method_type != NINGUNO))
					{
						if (call->bss_method_type == RSSI_NUC)
						{
							a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("NUCLEO")));
							pjmedia_sdp_media_add_attr(sdp->media[0], a);

							a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("RSSI")));
							pjmedia_sdp_media_add_attr(sdp->media[0], a);
						}
						else if (call->bss_method_type == RSSI)
						{
							a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("RSSI")));
							pjmedia_sdp_media_add_attr(sdp->media[0], a);
						}
						else if ((call->bss_method_type == CENTRAL) && (call->_Info.porcentajeRSSI > MIN_porcentajeRSSI))
						{
							//Si es CENTRALIZADO y se necesita un porcentaje de valor RSSI
							a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("RSSI")));
							pjmedia_sdp_media_add_attr(sdp->media[0], a);
						}
					}
				} 

				//if (call->_Info.PreferredBss == 2)
				//{
				//	a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("AGC")));
				//	pjmedia_sdp_media_add_attr(sdp->media[0], a);
				//}
				//else if (call->_Info.PreferredBss == 3)
				//{
				//	a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("C/N")));
				//	pjmedia_sdp_media_add_attr(sdp->media[0], a);
				//}
				//else if (call->_Info.PreferredBss == 4)
				//{
				//	a = pjmedia_sdp_attr_create(call->_Pool, "bss", &(pj_str("PSD")));
				//	pjmedia_sdp_media_add_attr(sdp->media[0], a);
				//}

				a = pjmedia_sdp_attr_create(call->_Pool, "interval", &(pj_str("20")));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);
				a = pjmedia_sdp_attr_create(call->_Pool, "sigtime", &(pj_str("1")));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);


				/*¡ATENCION! Si se modifica el valor de ptt_rep hay que modificar el valor de la etiqueta
				 * PTT_REP_COUNT en el fichero stream.c de pjmedia. Para ptt_rep=1 PTT_REP_COUNT debe valer 2,
				 * que es el numero de paquetes rtp con ptt off al finalizar un ptt on
				 */
				a = pjmedia_sdp_attr_create(call->_Pool, "ptt_rep", &(pj_str("1")));
				pjmedia_sdp_media_add_attr(sdp->media[0], a);


				/** AGL 1420528 */
				pj_str_t rtphe = pj_str("1");
				a = pjmedia_sdp_attr_create(call->_Pool, "rtphe",&rtphe);
				pjmedia_sdp_media_add_attr(sdp->media[0], a);

			}
			else if ((call->_Info.Type == CORESIP_CALL_MONITORING) || 
				(call->_Info.Type == CORESIP_CALL_GG_MONITORING) ||
				(call->_Info.Type == CORESIP_CALL_AG_MONITORING))
			{
				/*La llamada es del tipo monitoring (escucha)*/ 
				pjmedia_sdp_attr * attr = pjmedia_sdp_media_find_attr2(sdp->media[0], "sendrecv", NULL);
				if (attr != NULL)
				{
					if (callinfo.id != PJSUA_INVALID_ID && callinfo.role == PJSIP_ROLE_UAC)
					{
						//La llamada ya ha sido establecida y en este caso el rol es UAC
						attr->name = gRecvOnly;
					}
					else if (callinfo.id != PJSUA_INVALID_ID && callinfo.role == PJSIP_ROLE_UAS)
					{
						//La llamada ya ha sido establecida y en este caso el rol es UAS
						attr->name = gSendOnly;
					}
					else
					{
						//En este caso la llamada no ha sido establecida, nosotros somos los que actuamos de uac
						// es decir, los que llamamos
						attr->name = gRecvOnly;
					}
				}
			}
			else if (call->_Info.Type == CORESIP_CALL_IA && !SipAgent::EnableMonitoring)
			{
				pjmedia_sdp_attr * attr = pjmedia_sdp_media_find_attr2(sdp->media[0], "sendrecv", NULL);
				if (attr != NULL)
					attr->name = gSendOnly;
			}
		}
		else if (rdata && !SipAgent::EnableMonitoring)
		{
			pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &gSubjectHdr, NULL);
			if (subject && (pj_stricmp(&subject->hvalue, &gSubject[CORESIP_CALL_IA]) == 0))
			{
				pjmedia_sdp_attr * attr = pjmedia_sdp_media_find_attr2(sdp->media[0], "sendrecv", NULL);
				if (attr != NULL)
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

	
	pjsua_call_info callInfo;
	if (pjsua_call_get_info(call_id, &callInfo) != PJ_SUCCESS)
	{	
		return;
	}

	SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);
	if (!call) return;

	if (call->_Info.Type != CORESIP_CALL_IA)
	{
		//Los eventos de dialogo de llamada del tipo IA no se envian
		ExtraParamAccId::SendDialogNotifyFromAcc(call_id, DlgSubs::WITH_BODY);
	}

	CORESIP_CallStateInfo stateInfo = { (CORESIP_CallState)callInfo.state, (CORESIP_CallRole)callInfo.role };

	if (call->_HangUp)
	{
		if (callInfo.state == PJSIP_INV_STATE_DISCONNECTED)
		{
			if (call->_Info.Type == CORESIP_CALL_RD)
			{
				call->valid_sess = PJ_FALSE;						
				EliminarRadSessionDelGrupo(call);					
			}

			if (call->_Info.Type != CORESIP_CALL_RD)
			{
				SipAgent::RecCallEnd(Q931_NORMAL_CALL_CLEARING, CALLEND_UNKNOWN);
				SipAgent::RecBYETel();
			}
			else
			{
				SipAgent::RecBYERad();
			}

			// Debe mandarse al cliente info de la desconexi�n de la llamada
			pjsua_call_set_user_data(call_id, (void *) NULL);  //Este call id deja de tener un SipCall asociado				
			SipAgent::Cb.CallStateCb(call_id | CORESIP_CALL_ID, &call->_Info, &stateInfo);	

			call->IniciaFinSesion();
		}
		else
		{
			PJ_LOG(3,(__FILE__, "WARNING: OnStateChanged: Estado incorrecto despues de solicitar Hangup DstUri %s state %d %s.\n", call->DstUri, callInfo.state, callInfo.state_text.ptr));			
			if (callInfo.state == PJSIP_INV_STATE_CONFIRMED)
			{
				PJ_LOG(3,(__FILE__, "WARNING: OnStateChanged: Estado CONFIRMED despues de solicitar Hangup. Se fuerza Hangup DstUri %s state %d %s.\n", call->DstUri, callInfo.state, callInfo.state_text.ptr));			
				pj_status_t s1 = pjsua_call_hangup(call_id, 0, NULL, NULL);
				if (s1 != PJ_SUCCESS)
				{
					PJ_LOG(3,(__FILE__, "WARNING: OnStateChanged: pjsua_call_hangup retorna error 0x%X\n", s1));			
				}
			}			
		}						

		return;
	}


	if (callInfo.role == PJSIP_ROLE_UAC)
	{
		pjmedia_session * sess = pjsua_call_get_media_session(call_id);
			
		if (callInfo.state == PJSIP_INV_STATE_CONFIRMED)
		{
			if (call->_Info.Type == CORESIP_CALL_RD)
			{
				pjmedia_session_info sess_info;
					
				if (sess != NULL)
				{
					pjsua_call * psuacall = (pjsua_call *) pjmedia_session_get_user_data((pjmedia_session*)sess);
					pjmedia_transport_info tpinfo;
					pjmedia_transport_info_init(&tpinfo);
					pjmedia_transport_get_info(psuacall->med_tp, &tpinfo);
						
					pjmedia_session_get_info(sess, &sess_info);

					char *bss_method = NULL;
					if (sess_info.stream_cnt > 0)
					{
						stateInfo.PttId = (unsigned short)sess_info.stream_info[0].pttId;
						stateInfo.ClkRate = sess_info.stream_info[0].param->info.clock_rate;
						stateInfo.ChannelCount = sess_info.stream_info[0].param->info.channel_cnt;
						stateInfo.BitsPerSample = sess_info.stream_info[0].param->info.pcm_bits_per_sample;
						stateInfo.FrameTime = sess_info.stream_info[0].param->info.frm_ptime;

						bss_method = sess_info.stream_info[0].bss_method;

						pjmedia_stream * stream = NULL;
						stream = pjmedia_session_get_stream(sess, 0);
						if ((stream != NULL) && (call->_Info.Flags & CORESIP_CALL_RD_RXONLY))
						{
							pjmedia_stream_set_rx_only(stream, PJ_TRUE);
						}
					}						
						
					int n_sessions_in_his_group = SipAgent::_FrecDesp->AddToGroup(call->_RdFr, call->_Id, call, bss_method, call->_Info.Flags);
					if (n_sessions_in_his_group < 0)
					{
						PJ_LOG(3,(__FILE__, "ERROR: Freq %s, call id %d cannot be added to any CLIMAX group\n", call->_RdFr, call->_Id));
					}	
					else
					{
						/*Arrancamos en cualquier caso el Check_CLD_timer. Aunque no sea un grupo todavia*/

						/*Se para el timer de supervision CLD, por si estuviera arrancado*/
						call->Check_CLD_timer.id = Check_CLD_timer_IDLE;
						pjsua_cancel_timer(&call->Check_CLD_timer);

						call->valid_sess = PJ_TRUE;

						if (call->_Info.cld_supervision_time != 0)
						{
							/*Se arranca el timer de supervision CLD, siempre que cld_supervision_time no valga 0*/						
							pj_time_val	delay;
							delay.sec = 0;
							delay.msec = 10;		//Pide el MAM cuanto antes

							pj_timer_entry_init( &call->Check_CLD_timer, Check_CLD_timer_SEND_RMM, (void *) call, Check_CLD_timer_cb);
							pj_status_t st = pjsua_schedule_timer(&call->Check_CLD_timer, &delay);
							if (st != PJ_SUCCESS)
							{
								call->Check_CLD_timer.id = Check_CLD_timer_IDLE;
								PJ_CHECK_STATUS(st, ("ERROR en Check_CLD_timer"));
							}
						}

						call->Wait_init_timer.id = 0;
						pjsua_cancel_timer(&call->Wait_init_timer);

						call->Wait_init_timer.cb = Wait_init_timer_cb;
						call->Wait_init_timer.user_data = (void *) call;
						pj_time_val	delay1;
						delay1.sec = (long) WAIT_INIT_TIME_seg;
						delay1.msec = (long) WAIT_INIT_TIME_ms;	
						call->Wait_init_timer.id = 1;
						pj_status_t st = pjsua_schedule_timer(&call->Wait_init_timer, &delay1);
						if (st != PJ_SUCCESS)
						{
							call->Wait_init_timer.id = 0;
							PJ_CHECK_STATUS(st, ("ERROR en Wait_init_timer"));
						}							
					}
				}
			}					

		}
		else if (callInfo.state == PJSIP_INV_STATE_DISCONNECTED)
		{
			pjsua_call_set_user_data(call_id, (void *) NULL);  //Este call id deja de tener un SipCall asociado
			if (call->_Info.Type == CORESIP_CALL_RD)
			{
				call->valid_sess = PJ_FALSE;
				EliminarRadSessionDelGrupo(call);		
			}

			if ((callInfo.last_status == PJSIP_AC_AMBIGUOUS) && (call->_Info.Type == CORESIP_CALL_RD))
			{
				/*Se ha detectado que si las radios retornan el codigo ambiguous se debe a que tienen
				abierta alguna sesion zombie. Por tanto lo que se hace aquí es buscar todas las
				sesiones que existan abiertas con esa radio y cerrarlas*/

				pjsua_call_id call_ids[PJSUA_MAX_CALLS];
				unsigned call_cnt=PJ_ARRAY_SIZE(call_ids);

				pj_status_t sst = pjsua_enum_calls(call_ids, &call_cnt);
				if (sst == PJ_SUCCESS)
				{
					for (unsigned i = 0; i < call_cnt; i++)
					{
						if (call_ids[i] != call_id)
						{
							pjsua_call_info info;
							sst = pjsua_call_get_info(call_ids[i], &info);
							if (sst == PJ_SUCCESS)
							{
								//Todas las sesiones que existan abiertas a la misma uri se cierran
								if (pj_strcmp(&info.remote_info, &callInfo.remote_info) == 0)
								{
									//pjsua_call_hangup(call_ids[i], PJSIP_SC_DECLINE, &(pj_str("Closing zombie session")), NULL);
									Hangup(call_ids[i], PJSIP_AC_AMBIGUOUS);
								}
							}
						}
					}
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
	
	
	if (stateInfo.LastCode == PJSIP_SC_DECLINE)
	{
		// Comprobar si el DECLINE viene como consecuencia de que la pasarela no es el activa
		pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(e->body.tsx_state.src.rdata->msg_info.msg, &_gSubjectHdr, NULL);
		if (subject)
		{
			pj_ansi_snprintf(call->LastReason, sizeof(call->LastReason) - 1, "%.*s", 
				subject->hvalue.slen, subject->hvalue);
			strcpy(stateInfo.LastReason, call->LastReason);
		}
	}
	else if (callInfo.state == PJSIP_INV_STATE_DISCONNECTED)
	{
		if (strlen(call->LastReason) == 0)
		{
			//Si finalizada la sesion call->LastReason ya tiene contenido entonces es porque ya ha llegado en el
			//CANCEL, que es tratado en OnTsxStateChanged()
			//Aquí nos aseguramos que la sesion a terminado por un BYE
			if (
				(callInfo.media_status != PJSUA_CALL_MEDIA_NONE && callInfo.media_status != PJSUA_CALL_MEDIA_ERROR) &&
				(callInfo.connect_duration.msec != 0 || callInfo.connect_duration.sec != 0)
				)
			{
				if (e->body.rx_msg.rdata != NULL)
				{
					if (e->body.rx_msg.rdata->msg_info.msg != NULL)
					{
						pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(e->body.rx_msg.rdata->msg_info.msg, &_gSubjectHdr, NULL);
						if (subject != NULL)
						{					
							if (call)
							{
								pj_ansi_snprintf(call->LastReason, sizeof(call->LastReason) - 1, "%.*s", 
									subject->hvalue.slen, subject->hvalue);
							}						
						}
					}
				}
			}
		}

		strcpy(stateInfo.LastReason, call->LastReason);
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
			char dst[CORESIP_MAX_URI_LENGTH];
			strncpy(dst, callInfo.remote_info.ptr, CORESIP_MAX_URI_LENGTH);
			if (callInfo.remote_info.slen < (CORESIP_MAX_URI_LENGTH-1))
				dst[callInfo.remote_info.slen] = '\0';
			else
				dst[CORESIP_MAX_URI_LENGTH-1] = '\0';
			if (SipAgent::_ConfManager->GetSubsObj(dst) == NULL)
			{
				//Se subscribe a la conferencia siempre y cuando el focus no sea un multidestino
				//Porque en ese caso ya estaría subscrito fuera de este dialogo

				call->SubscribeToConfInfo();				
			}
		}
		else if (!callInfo.remote_focus && call->_ConfInfoClientEvSub)
		{
			pjsip_tx_data * tdata;
			if (PJ_SUCCESS == pjsip_conf_initiate(call->_ConfInfoClientEvSub, 0, &tdata))
			{
				pjsip_conf_send_request(call->_ConfInfoClientEvSub, tdata);
			}
		}

		if (call->_Info.Type != CORESIP_CALL_RD)
		{
			SipAgent::RecCallConnected(&callInfo.remote_info);	
		}	
	}
	else if (callInfo.state == PJSIP_INV_STATE_DISCONNECTED)
	{			
		if (call->_Info.Type == CORESIP_CALL_RD)
		{
			SipAgent::RecBYERad();
		}
		else
		{
			SipAgent::RecBYETel();
		}

		if (call->_Info.Type != CORESIP_CALL_RD)
		{				
			SipAgent::RecCallEnd(Q931_NORMAL_CALL_CLEARING, CALLEND_DEST);
			SipAgent::RecBYETel();
		}
		else
		{
			SipAgent::RecBYERad();
		}		

		call->IniciaFinSesion();
	}
}

void SipCall::EliminarRadSessionDelGrupo(SipCall *call)
{
	if (call == NULL) return;	

	call->Wait_init_timer.id = 0;
	pjsua_cancel_timer(&call->Wait_init_timer);

	call->Check_CLD_timer.id = Check_CLD_timer_IDLE;
	pjsua_cancel_timer(&call->Check_CLD_timer);

	if (call->window_timer.id == 1)
	{
		SipAgent::_FrecDesp->SetInWindow(call->_Index_group, PJ_FALSE);
	}
	call->window_timer.id = 0;
	pjsua_cancel_timer(&call->window_timer);

	if (call->out_circbuff_thread != NULL)
	{
		call->out_circbuff_thread_run = PJ_FALSE;
		pj_sem_post(call->sem_out_circbuff);
		pj_thread_join(call->out_circbuff_thread);
		pj_thread_destroy(call->out_circbuff_thread);
		call->out_circbuff_thread = NULL;
	}

	if (call->_Index_group < 0 || call->_Index_sess < 0 || 
		call->_Index_group >= FrecDesp::MAX_GROUPS || call->_Index_sess >= FrecDesp::MAX_SESSIONS)
	{
		return;
	}	
					
	pj_sockaddr_in *RdSndTo;
	SipAgent::_FrecDesp->Get_group_multicast_socket(call->_Index_group, &RdSndTo);
	if (RdSndTo != NULL && RdSndTo == &call->_RdSendTo)
	{
		//El grupo est� enviando por multicast a la misma sockaddr que esta sesion que ahora se elimina
		SipAgent::_FrecDesp->Set_group_multicast_socket(call->_Index_group, NULL);
	}	

	int n_sessions_in_his_group = SipAgent::_FrecDesp->RemFromGroup(call->_Index_group, call->_Index_sess);
	if (n_sessions_in_his_group < 0)
	{
		//PJ_LOG(3,(__FILE__, "ERROR: Freq %s, call id %d cannot be removed from any CLIMAX group\n", call->_RdFr, call->_Id));
	}	
	else if (n_sessions_in_his_group > 0)
	{
		if (call->_Sending_Multicast_enabled)
		{   //Esta sesion que se va a eliminar es la activa en el grupo bss y por tanto se va a cortar el audio
			//Forzamos un estado de ningun squelch activado en el grupo para forzar el arranque de una nueva ventana de decision
			SipAgent::_FrecDesp->EnableMulticast(call, PJ_FALSE, PJ_TRUE);
			SipAgent::_FrecDesp->SetSelected(call, PJ_FALSE, PJ_TRUE);
			SipAgent::_FrecDesp->ClrSelectedUri(call);
			SipAgent::_FrecDesp->ClrAllSquSt(call->_Index_group);
		}
	}

	//Se reinician los indices del grupo
	call->_Index_group = FrecDesp::INVALID_GROUP_INDEX;
	call->_Index_sess = FrecDesp::INVALID_SESS_INDEX;
}

/*Timer de espera despues de iniciarse la sesi�n*/
void SipCall::Wait_init_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
	SipCall *wp = (SipCall *)te->user_data;

	if (wp->Wait_init_timer.id == 0)
	{
		pjsua_cancel_timer(&wp->Wait_init_timer);
		return;
	}

	wp->Wait_init_timer.id = 0;
	pjsua_cancel_timer(&wp->Wait_init_timer);
	if (!wp->valid_sess) return;
	
	pjmedia_session *sess = pjsua_call_get_media_session(wp->_Id);
	if (sess != NULL)
		pjmedia_session_reset_ext_header(sess);
}

/*Timer que arranca despues de un ptt off*/
void SipCall::Ptt_off_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
	SipCall *wp = (SipCall *)te->user_data;

	if (wp->Ptt_off_timer.id == 0)
	{
		pjsua_cancel_timer(&wp->Ptt_off_timer);
		return;
	}

	wp->Ptt_off_timer.id = 0;
	pjsua_cancel_timer(&wp->Ptt_off_timer);	

	//Se actualiza en la aplicacion es estado rdinfo
	CORESIP_RdInfo info_aux;
	pj_mutex_lock(wp->RdInfo_prev_mutex);
	memcpy(&info_aux, &wp->RdInfo_prev, sizeof(CORESIP_RdInfo));
	pj_mutex_unlock(wp->RdInfo_prev_mutex);

	//Actualizamos los parametros que no se toman en la callback OnRdInfochanged
	//Esto hay que hacerlo siempre que se llame a RdInfoCb
	//-->
	info_aux.rx_selected = SipAgent::_FrecDesp->IsBssSelected(wp);
	//<--

	if (info_aux.Squelch == 0) info_aux.rx_qidx = 0;	//Si no hay squelch el qidx que se envia a la aplicacion es cero
	if (SipAgent::Cb.RdInfoCb)
	{
		PJ_LOG(5,(__FILE__, "SipCall::Ptt_off_timer_cb: Fin timer. Envia a nodebox dst %s PttType %d PttId %d rx_selected %d Squelch %d", wp->DstUri, info_aux.PttType, info_aux.PttId, info_aux.rx_selected, info_aux.Squelch));
		SipAgent::Cb.RdInfoCb(wp->_Id | CORESIP_CALL_ID, &info_aux);
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

	CORESIP_CallFlags TipoGRS = CORESIP_CALL_NINGUNO;	

	if (SipAgent::Cb.CallIncomingCb)
	{
		CORESIP_CallInfo info = { acc_id | CORESIP_ACC_ID, CORESIP_CALL_UNKNOWN, CORESIP_PR_UNKNOWN };

		info.DestinationPort = 0;
		info.Flags_type = CORESIP_CALL_NINGUNO;
		info.Flags = CORESIP_CALL_NINGUNO;

		pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &gSubjectHdr, NULL);
		pjsip_priority_hdr * priority = (pjsip_priority_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &gPriorityHdr, NULL);

		if (!subject && !priority)
		{
			//Si no se reciben cabeceras Subject y Priority. Se simula una llamada normal.
			//Así serán admitidas llamadas desde telefonos IP comerciales.
			info.Type = CORESIP_CALL_DIA;
			info.Priority = CORESIP_PR_NORMAL;
		}
		else
		{		
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
		}

		pjsip_uri *dst_uri;	//Uri del destino de la llamada entrante

		//Primero probamos que la uri del To corresponde con la de algun account
		//Si no, entonces probamos con la del request line
		if (pjsua_acc_find_for_incoming_by_uri(rdata->msg_info.to->uri) != PJSUA_INVALID_ID)
		{
			dst_uri = rdata->msg_info.to->uri;
		}
		else if (pjsua_acc_find_for_incoming_by_uri(rdata->msg_info.msg->line.req.uri) != PJSUA_INVALID_ID)
		{
			dst_uri = rdata->msg_info.msg->line.req.uri;
		}
		else
		{
			pjsua_call_answer(call_id, PJSIP_SC_NOT_FOUND, NULL, NULL);
			return;
		}

		if ((info.Type == CORESIP_CALL_UNKNOWN) || (info.Priority == CORESIP_PR_UNKNOWN) ||
			((info.Type == CORESIP_CALL_IA) && (info.Priority != CORESIP_PR_URGENT)) ||
			(!PJSIP_URI_SCHEME_IS_SIP(dst_uri) && !PJSIP_URI_SCHEME_IS_SIPS(dst_uri)) ||
			(!PJSIP_URI_SCHEME_IS_SIP(rdata->msg_info.from->uri) && !PJSIP_URI_SCHEME_IS_SIPS(rdata->msg_info.from->uri)))
		{
			pjsua_call_answer(call_id, PJSIP_SC_BAD_REQUEST, NULL, NULL);
			return;
		}

		pjmedia_sdp_session *rem_sdp;
		pj_status_t status = pjmedia_sdp_parse(rdata->tp_info.pool, (char*)rdata->msg_info.msg->body->data, rdata->msg_info.msg->body->len, &rem_sdp);
		if (status == PJ_SUCCESS) 
		{
			/* Validate */
			status = pjmedia_sdp_validate(rem_sdp);
		}

		if (status != PJ_SUCCESS)
		{
			rem_sdp = NULL;
		}

		if (rem_sdp != NULL) 
		{
			if (rem_sdp->media_count > 0)
			{
				for (unsigned int i = 0; i < rem_sdp->media_count; i++)
				{				
					pjmedia_sdp_attr *a;
					if ((a=pjmedia_sdp_media_find_attr2(rem_sdp->media[i], "port", NULL)) != NULL)
					{
						//Necesario para ETM
						info.DestinationPort=rem_sdp->media[i]->desc.port;
						break;
					}
				}
			}
		}


		pj_bool_t rdAccount = PJ_FALSE;				//Indica si add_id es un account tipo radio GRS

		ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(acc_id);
		if (extraParamAccCfg != NULL)
		{
			rdAccount = extraParamAccCfg->rdAccount;
		}

		if(info.Type == CORESIP_CALL_RD)
		{
			//Recibimos una llamada del tipo radio. 			

			if (!rdAccount)
			{
				//Rechazamos una llamada dirigida a una radio GRS si este account no es de este tipo
				pjsua_call_answer(call_id, PJSIP_SC_BAD_REQUEST, NULL, NULL);
				return;
			}

			if (rem_sdp != NULL) 
			{
				if (rem_sdp->media_count > 0)
				{
					for (unsigned int i = 0; i < rem_sdp->media_count; i++)
					{				
						pjmedia_sdp_attr *a;
						if ((a=pjmedia_sdp_media_find_attr2(rem_sdp->media[i], "type", NULL)) != NULL)
						{
							if(!pj_stricmp(&(a->value), &(pj_str("Radio"))))			
				 				info.Flags_type=CORESIP_CALL_RD_TXRX;
							else if(!pj_stricmp(&(a->value), &(pj_str("Radio-TxRx"))))			
				 				info.Flags_type=CORESIP_CALL_RD_TXRX;
							else if(!pj_stricmp(&(a->value), &(pj_str("Radio-Idle"))))		
				 				info.Flags_type=CORESIP_CALL_RD_IDLE;
							else if(!pj_stricmp(&(a->value), &(pj_str("Radio-Rxonly"))))			
				 				info.Flags_type=CORESIP_CALL_RD_RXONLY;
						}


						if ((a=pjmedia_sdp_media_find_attr2(rem_sdp->media[i], "txrxmode", NULL)) != NULL)
						{
							if(!pj_stricmp(&(a->value), &(pj_str("Rx"))))			
				 				info.Flags=CORESIP_CALL_RD_RXONLY;
							else if(!pj_stricmp(&(a->value), &(pj_str("Tx"))))		
				 				info.Flags=CORESIP_CALL_RD_TXONLY;
							else
				 				info.Flags=CORESIP_CALL_NINGUNO;
						}
						else
						{
							info.Flags=CORESIP_CALL_NINGUNO;
						}
					}
				}
			}
		}

		SipCall * call = new SipCall(call_id, &info);

		if (SipAgent::_Radio_UA || rdAccount)
		{
			//Este es un agente radio o un account del tipo radio GRS

			if (rem_sdp != NULL) 
			{
				if (rem_sdp->media_count > 0)
				{
					for (unsigned int i = 0; i < rem_sdp->media_count; i++)
					{				
						pjmedia_sdp_attr *a;

						if ((a=pjmedia_sdp_media_find_attr2(rem_sdp->media[i], "bss", NULL)) != NULL)
						{
							//El SDP incluye el campo bss, por tanto tenemos que enviar el qidx con el rtp
							call->_EnviarQidx = PJ_TRUE;
						}
					}
				}
			}
		}

		pjsua_call_set_user_data(call_id, call);

		CORESIP_CallInInfo inInfo = { };
		pjsip_sip_uri * src = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.from->uri);
		pjsip_sip_uri * dst = (pjsip_sip_uri*)pjsip_uri_get_uri(dst_uri);
		pj_str_t name = pjsip_uri_get_display(rdata->msg_info.from->uri);
		
		pj_ansi_snprintf(inInfo.SrcId, sizeof(inInfo.SrcId) - 1, "%.*s", src->user.slen, src->user.ptr);
		pj_ansi_snprintf(inInfo.SrcIp, sizeof(inInfo.SrcIp) - 1, "%.*s", src->host.slen, src->host.ptr);
		pj_ansi_snprintf(inInfo.DstId, sizeof(inInfo.DstId) - 1, "%.*s", dst->user.slen, dst->user.ptr);
		pj_ansi_snprintf(inInfo.DstIp, sizeof(inInfo.DstIp) - 1, "%.*s", dst->host.slen, dst->host.ptr);

		//Display name that comes optionally in SIP uri between quotation marks
		if (name.slen > 0)
		   pj_ansi_snprintf(inInfo.DisplayName, sizeof(inInfo.DisplayName) - 1, "%.*s", name.slen, name.ptr);

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

		//Para encontrar el puerto de origen buscamos una cabecera Record-route que contenga el mismo host que el From 
		pjsip_rr_hdr *rr = NULL;
		int com = -1;
		int port = 0;
		pjsip_sip_uri *rr_uri = NULL;
		int counter = 256;
		do {
			if (rr != NULL) rr = rr->next;
			rr = (pjsip_rr_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_RECORD_ROUTE, rr);
			if (rr != NULL) rr_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(&rr->name_addr);
			if (rr_uri != NULL) com = pj_strcmp(&rr_uri->host, &src->host);
			counter--;
		} while (rr != NULL && com != 0 && counter > 0);

		if (com == 0)
		{
			//Se ha encontrado un Record-route con el host igual al del from
			port = rr_uri->port;
			if (port == 0)
			{
				//no contiene el puerto. Será el de por defecto
				port = 5060;
			}
		}
		else
		{
			//No ha encontrado ninguna cabecera Record-route en la que coincida el host
			//Buscamos el del contact
			pjsip_contact_hdr *contact = (pjsip_contact_hdr*)  pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, NULL);
			if (contact != NULL)
			{
				pjsip_sip_uri *contact_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(contact->uri);
				if (contact_uri != NULL)
				{
					port = contact_uri->port;
					if (port == 0) port = 5060;	//no contiene el puerto. Será el de por defecto
				}
				else
				{
					//El contact no tiene uri
					pjsua_call_answer(call_id, PJSIP_SC_BAD_REQUEST, NULL, NULL);
					return;
				}
			}
			else
			{
				//NO hay contact
				pjsua_call_answer(call_id, PJSIP_SC_BAD_REQUEST, NULL, NULL);
				return;
			}
		}

		inInfo.SrcPort = (unsigned) port;

		//Para extraer la uri de destino
		pjsua_acc_info info_acc_id;
		pjsua_acc_get_info(acc_id, &info_acc_id);

		//Paa extraer la uri origen
		pjsua_call_info info_call_id;
		pjsua_call_get_info(call_id, &info_call_id);

		//Envia mensaje CallStart al grabador
		if (info.Type != CORESIP_CALL_RD)
		{
			SipAgent::RecINVTel();
			SipAgent::RecCallStart(SipAgent::INCOM, info.Priority, &info_call_id.remote_contact, &info_acc_id.acc_uri);
		}
		else
		{
			SipAgent::RecINVRad();
		}

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
					char dst[CORESIP_MAX_URI_LENGTH];
					strncpy(dst, callInfo.remote_info.ptr, CORESIP_MAX_URI_LENGTH);
					if (callInfo.remote_info.slen < (CORESIP_MAX_URI_LENGTH-1))
						dst[callInfo.remote_info.slen] = '\0';
					else
						dst[CORESIP_MAX_URI_LENGTH-1] = '\0';
					if (SipAgent::_ConfManager->GetSubsObj(dst) == NULL)
					{
						//Se subscribe a la conferencia siempre y cuando el focus no sea un multidestino
						//Porque en ese caso ya estaría subscrito fuera de este dialogo

						call->SubscribeToConfInfo();				
					}
				}
				else if (!callInfo.remote_focus && call->_ConfInfoClientEvSub)
				{
					pjsip_tx_data * tdata;
					if (PJ_SUCCESS == pjsip_conf_initiate(call->_ConfInfoClientEvSub, 0, &tdata))
					{
						pjsip_conf_send_request(call->_ConfInfoClientEvSub, tdata);
					}
				}

				if (call->_Info.Type != CORESIP_CALL_RD)
				{
					if (stateInfo.Role == CORESIP_CALL_ROLE_UAC)			//Este agente es el llamante
					{
						if (callInfo.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD)
						{
							SipAgent::RecHold(true, true);
						}
						else if (callInfo.media_status == PJSUA_CALL_MEDIA_ACTIVE)
						{
							SipAgent::RecHold(false, true);
						}
						else if (callInfo.media_status == CORESIP_MEDIA_REMOTE_HOLD ||
							callInfo.media_status == PJSUA_CALL_MEDIA_NONE) 
						{
							SipAgent::RecHold(true, false);
						}
					}
					else												//Este agente es el llamado
					{
						if (callInfo.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD)
						{
							SipAgent::RecHold(true, false);
						}
						else if (callInfo.media_status == PJSUA_CALL_MEDIA_ACTIVE)
						{
							SipAgent::RecHold(false, false);
						}
						else if (callInfo.media_status == CORESIP_MEDIA_REMOTE_HOLD ||
							callInfo.media_status == PJSUA_CALL_MEDIA_NONE) 
						{
							SipAgent::RecHold(true, true);
						}										
					}
				}
			}
		}
	}
}

/**
* Trata la notificaci�n de transferencia de llamada.
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
	
	static pj_str_t _gSubjectHdr = { "Reason", 6 };

	if ((tsx->role == PJSIP_ROLE_UAS) && (tsx->state == PJSIP_TSX_STATE_TRYING) &&
		 (pjsip_method_cmp(&tsx->method, pjsip_get_cancel_method()) == 0)) 		
	{
		//Se ha recibido un CANCEL

		pjsua_call_info callInfo;
		if (pjsua_call_get_info(call_id, &callInfo) == PJ_SUCCESS)
		{	
			if (e->body.rx_msg.rdata != NULL)
			{
				if (e->body.rx_msg.rdata->msg_info.msg != NULL)
				{
					pjsip_subject_hdr * subject = (pjsip_subject_hdr*)pjsip_msg_find_hdr_by_name(e->body.rx_msg.rdata->msg_info.msg, &_gSubjectHdr, NULL);
					if (subject != NULL)
					{
						SipCall * sipcall = (SipCall*)pjsua_call_get_user_data(call_id);
						if (sipcall)
						{
							pj_ansi_snprintf(sipcall->LastReason, sizeof(sipcall->LastReason) - 1, "%.*s", 
								subject->hvalue.slen, subject->hvalue);
						}
					}
				}
			}
		}
	}
	else if ((tsx->role == PJSIP_ROLE_UAS) && (tsx->state == PJSIP_TSX_STATE_TRYING) &&
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
				if (call->_ConfInfoSrvEvSub)
				{
					PJ_LOG(5, ("sipcall.cpp", "NOTIFY CONF: Recibe conference SUBSCRIBE STATE %s", pjsip_evsub_get_state_name(call->_ConfInfoSrvEvSub)));
				}
				
				if (!call->_ConfInfoSrvEvSub)
				{
					st = pjsip_conf_create_uas(dlg, &call->_ConfInfoSrvCb, rdata, &call->_ConfInfoSrvEvSub);
					if (st != PJ_SUCCESS)
					{
						pjsua_perror("sipcall.cpp", "ERROR creando servidor de subscripcion a conferencia", st);
						goto subscription_error;
					}
					//pjsip_evsub_set_mod_data(call->_ConfInfoSrvEvSub, pjsua_var.mod.id, call);	
					pjsip_evsub_set_user_data(call->_ConfInfoSrvEvSub, (void *) call);

					PJ_LOG(5, ("sipcall.cpp", "NOTIFY CONF: Servidor SUBSCRIBE creado"));

				}							

				st = pjsip_conf_accept(call->_ConfInfoSrvEvSub, rdata, PJSIP_SC_OK, NULL);
				if (st != PJ_SUCCESS)
				{
					pjsua_perror("sipcall.cpp", "ERROR aceptando subscripcion a conferencia", st);
					goto subscription_error;
				}

				pjsip_evsub_set_state(call->_ConfInfoSrvEvSub, PJSIP_EVSUB_STATE_ACCEPTED);				

				pjsua_call_info info;
				st = pjsua_call_get_info(call_id, &info);
				if (st == PJ_SUCCESS && SipAgent::Cb.IncomingSubscribeConfCb)
				{
					SipAgent::Cb.IncomingSubscribeConfCb(call_id | CORESIP_CALL_ID, (const char *) info.remote_info.ptr, (const int) info.remote_info.slen);
				}
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
* Gestiona la creaci�n de los stream y su conexion a los puertos de conferencia
* http://www.pjsip.org/docs/latest-1/pjsip/docs/html/structpjsua__callback.htm#a88e61b65e936271603e5d63503aebcf6
*/
void SipCall::OnStreamCreated(pjsua_call_id call_id, pjmedia_session * sess, unsigned stream_idx, pjmedia_port **p_port)
{
	SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);
	
	if (call && (call->_Info.Flags & CORESIP_CALL_EC))
	{		
		pjmedia_session_enable_ec(sess, stream_idx, SipAgent::EchoTail, SipAgent::EchoLatency, 0);							// AGL. 20131120. Par�metros Configurables.
	}
}

/*
Se llama en stop_media_session, justo antes de destruir session. Me servirá
para eliminarlo del grupo climax
*/

void SipCall::OnStreamDestroyed(pjsua_call_id call_id, pjmedia_session *sess, unsigned stream_idx)
{
	SipCall * call = (SipCall*)pjsua_call_get_user_data(call_id);

	if (call != NULL)
	{
		if (call->_Info.Type == CORESIP_CALL_RD)
		{
			EliminarRadSessionDelGrupo(call);
		}	
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
	//SipCall * call = (SipCall*)pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
	SipCall * call = (SipCall*) pjsip_evsub_get_user_data(sub);
	if (call) 
	{
		PJ_LOG(4, ("sipcall.cpp", "Conference server subscription is %s", pjsip_evsub_get_state_name(sub)));

		if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) 
		{
			call->_ConfInfoSrvEvSub = NULL;
			pjsip_evsub_set_user_data(sub, NULL);
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

		pjsip_conf_status conf_info;

		if (PJ_SUCCESS == pjsip_conf_get_status(sub, &conf_info))
		{
			char from_urich[CORESIP_MAX_URI_LENGTH];
			pjsip_from_hdr *from_hdr = (pjsip_from_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_FROM, NULL);
			int size = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, from_hdr->uri, from_urich, CORESIP_MAX_URI_LENGTH-1);
			from_urich[size] = '\0';

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

			if (call)
			{
				SipAgent::Cb.ConfInfoCb(call->_Id | CORESIP_CALL_ID, &info, from_urich, size);
			}
			else
			{
				//SipAgent::Cb.ConfInfoCb(-1, &info, from_urich, size);
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

/** 
Se llama cada vez que se va a transmitir un REQUEST SIP.
*/
pj_bool_t SipCall::OnTxRequest(pjsip_tx_data *txdata)
{
	pjsip_msg *msg = txdata->msg;
	pj_bool_t gWG67VersionRadio = PJ_FALSE;

	if (SipAgent::IsRadio || SipAgent::_Radio_UA)
	{
		//Es un agente Radio
		gWG67VersionRadio = PJ_TRUE;		 
	}
	else if (SipAgent::_HaveRdAcc)
	{
		//No es un agente radio, pero puede ser que el account que envia el mensaje si lo sea
		//Esto es posible en el ETM

		pjsua_acc_id acc_id = PJSUA_INVALID_ID;

		if (msg->type == PJSIP_REQUEST_MSG)
		{
			//Si el mensaje es un request, entonces comprobamos si la uri del from corresponde
			//a algun account del agente. 
			pjsip_from_hdr* from = (pjsip_from_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL);
			if (from != NULL)
			{
				//char buff[64];
				//pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, from->uri, buff, sizeof(buff));

				acc_id = pjsua_acc_find_for_incoming_by_uri(from->uri);
			}
		}
		else
		{
			//Si el mensaje es una respuesta, entonces comprobamos si la uri del to corresponde
			//a algun account del agente. 
			pjsip_to_hdr* to = (pjsip_to_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL);
			if (to != NULL)
			{
				//char buff[64];
				//pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, to->uri, buff, sizeof(buff));

				acc_id = pjsua_acc_find_for_incoming_by_uri(to->uri);
			}
		}

		if (acc_id != PJSUA_INVALID_ID)
		{
			//Con el account id tomamos los ExtraParamAccId y de ahi se obtiene si la cuenta es del tipo GRS
			pj_bool_t rdAccount = PJ_FALSE;				//Indica si acc_id es un account tipo radio GRS
			ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(acc_id);
			if (extraParamAccCfg != NULL)
			{
				rdAccount = extraParamAccCfg->rdAccount;
				if (rdAccount)	
				{
					gWG67VersionRadio = PJ_TRUE;
				}
			}
		}	
	}

	if (gWG67VersionRadio)
	{
		/** WG67-Version Header */
		Wg67VersionSet(txdata, &gWG67VersionRadioValue);

		/** BYE Request WG67-Reason Header */
		if (txdata->msg->line.req.method.id == PJSIP_BYE_METHOD)
		{			
			if (WG67Reason != 0)
			{
				Wg67ReasonSet(txdata);
				WG67Reason = 0;
			}
		}		

		/** RE-INVITE. Vienen sin la prioridad */
		if (txdata->msg->line.req.method.id == PJSIP_INVITE_METHOD)
		{
			Wg67RadioPrioritySet(txdata);
			Wg67RadioSubjectSet(txdata);
		}
	}
	else 
	{
		/** WG67-Version Header */
		Wg67VersionSet(txdata, &gWG67VersionTelefValue);

		/** 200 OK. Confirmaciones de Llamada. A�adimos CONTACT, ALLOW, SUPPORTED ... */
		//if (txdata->msg->line.status.code==200)
		//{
			//Wg67ContactSet(txdata);			//EDU 26/01/2017. El contact no va en todos los mensajes sip segun ED137
												//PJSIP ya a�ade los necesarios
			//Wg67AllowSet(txdata);				//EDU 26/01/2017. El allow no va en todos los mensajes sip segun ED137
												//PJSIP ya a�ade los necesarios
			//Wg67SupportedSet(txdata);			//EDU 26/01/2017. El supported no va en todos los mensajes sip segun ED137
												//PJSIP ya a�ade los necesarios
		//}
	}

	

	return PJ_FALSE;
}

void SipCall::SetImpairments(pjsua_call_id call_id, CORESIP_Impairments * impcfg)
{
	pjsua_call * call;
	pjsip_dialog * dlg;
	pj_status_t st;

	st = acquire_call("SetImpairments()", call_id, &call, &dlg);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3, ("SipCall.cpp", "ERROR: SetImpairments: Invalid call_id %d", call_id));
		return;
	}
	
	pjmedia_session* session = pjsua_call_get_media_session(call_id);
	if (session == NULL) return; 

	pjmedia_stream * stream = NULL;
	stream = pjmedia_session_get_stream(session, 0);
	if (stream != NULL)
		pjmedia_stream_force_set_impairments(stream, impcfg->Perdidos, impcfg->Duplicados, impcfg->LatMin, impcfg->LatMax);

	pjsip_dlg_dec_lock(dlg);
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
