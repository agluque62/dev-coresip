#include "Global.h"
#include "wg67subscription.h"
#include "exceptions.h"

#undef THIS_FILE
#define THIS_FILE		"wg67subscription.cpp"

CORESIP_Callbacks WG67Subscription::_Cb;

void WG67Subscription::Init(pj_pool_t * pool, const CORESIP_Config * cfg)
{
	_Cb = cfg->Cb;
}

void WG67Subscription::End()
{
}

void WG67Subscription::wg67_on_state(pjsip_evsub *sub, pjsip_event *event)
{
	PJ_UNUSED_ARG(event);

	WG67Subscription * wg67 = reinterpret_cast<WG67Subscription*>(pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id));

	if (wg67) 
	{
		PJ_LOG(4,(THIS_FILE, "WG67KEY-IN subscription to %.*s is %s",
			wg67->_Dst.slen, wg67->_Dst.ptr, pjsip_evsub_get_state_name(sub)));

		if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) 
		{
			CORESIP_WG67Info info;
			const pj_str_t * reason = pjsip_evsub_get_termination_reason(sub);

			info.SubscriptionTerminated = 1;
			info.SubscribersCount = 0;
			pj_ansi_sprintf(info.DstUri, "%.*s", wg67->_Dst.slen, wg67->_Dst.ptr);
			pj_ansi_sprintf(info.LastReason, "%.*s", reason->slen, reason->ptr);

			if (_Cb.WG67NotifyCb)
				_Cb.WG67NotifyCb(wg67, &info, _Cb.UserData);

			wg67->_Module = NULL;
			pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
		} 
	}
}

void WG67Subscription::wg67_on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, 
													  pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body)
{
	WG67Subscription * wg67 = reinterpret_cast<WG67Subscription*>(pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id));

	if (wg67) 
	{
		pjsip_wg67_status wg67_info;
		pjsip_wg67_get_status(sub, &wg67_info);

		CORESIP_WG67Info info;

		info.SubscribersCount = info.SubscriptionTerminated = 0;
		//info.SubscribersCount = wg67_info.info_cnt;
		pj_ansi_sprintf(info.DstUri, "%.*s", wg67->_Dst.slen, wg67->_Dst.ptr);

		for (unsigned i = 0; i < wg67_info.info_cnt; i++)
		{
			pjsip_sip_uri * uri = (pjsip_sip_uri*)wg67_info.info[i].sipid.uri;

			info.Info[info.SubscribersCount].PttId = wg67_info.info[i].pttid;
			pj_ansi_sprintf(info.Info[info.SubscribersCount].SubsUri, "%.*s@%.*s", 
				uri->user.slen, uri->user.ptr, uri->host.slen, uri->host.ptr);

			// PlugTest FAA 05/2011
			info.SubscribersCount++;
		}

		if (_Cb.WG67NotifyCb)
			_Cb.WG67NotifyCb(wg67, &info, _Cb.UserData);
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

pjsip_evsub_user WG67Subscription::wg67_callback = 
{
	&WG67Subscription::wg67_on_state,  
	NULL,
	NULL,
	&WG67Subscription::wg67_on_rx_notify,
	NULL,
	NULL
};

WG67Subscription::WG67Subscription(const char * dst)
: _Pool(NULL), _Module(NULL)
{
	_Pool = pjsua_pool_create(NULL, 512, 256);
	pj_strdup2_with_null(_Pool, &_Dst, dst);

	pjsip_dialog * dlg = NULL;
	pj_status_t st;

	try
	{
		pjsua_acc_id acc_id = pjsua_acc_get_default();
		pjsua_acc * acc = &pjsua_var.acc[acc_id];

		pj_str_t contact;
		if (acc->contact.slen) 
		{
			contact = acc->contact;
		} 
		else 
		{
			st = pjsua_acc_create_uac_contact(_Pool, &contact, acc_id, &_Dst, NULL);
			PJ_CHECK_STATUS(st, ("Unable to generate Contact header for WG67KEY-IN"));
		}

		st = pjsip_dlg_create_uac(pjsip_ua_instance(), &acc->cfg.id, &contact, &_Dst, NULL, &dlg);
		PJ_CHECK_STATUS(st, ("Unable to create dialog for WG67KEY-IN"));

		st = pjsip_wg67_create_uac(dlg, &wg67_callback, PJSIP_EVSUB_NO_EVENT_ID, &_Module);
		PJ_CHECK_STATUS(st, ("Unable to create WG67KEY-IN client"));

		if (acc->cfg.transport_id != PJSUA_INVALID_ID) 
		{
			pjsip_tpselector tp_sel;

			pjsua_init_tpselector(acc->cfg.transport_id, &tp_sel);
			pjsip_dlg_set_transport(dlg, &tp_sel);
		}

		if (!pj_list_empty(&acc->route_set)) 
		{
			pjsip_dlg_set_route_set(dlg, &acc->route_set);
		}

		if (acc->cred_cnt) 
		{
			pjsip_auth_clt_set_credentials(&dlg->auth_sess, acc->cred_cnt, acc->cred);
		}
		pjsip_auth_clt_set_prefs(&dlg->auth_sess, &acc->cfg.auth_pref);

		pjsip_evsub_set_mod_data(_Module, pjsua_var.mod.id, this);

		pjsip_tx_data *tdata;
		st = pjsip_wg67_initiate(_Module, -1, &tdata);
		PJ_CHECK_STATUS(st, ("Unable to create initial WG67KEY-IN SUBSCRIBE"));

		pjsua_process_msg_data(tdata, NULL);

		st = pjsip_wg67_send_request(_Module, tdata);
		PJ_CHECK_STATUS(st, ("Unable to send initial WG67KEY-IN SUBSCRIBE"));
	}
	catch (...)
	{
		if (_Module)
		{
			pjsip_wg67_terminate(_Module, PJ_FALSE);
		}
		else if (dlg)
		{
			pjsip_dlg_terminate(dlg);
		}
		if (_Pool)
		{
			pj_pool_release(_Pool);
		}
		throw;
	}	
}

WG67Subscription::~WG67Subscription()
{
	if (_Module != NULL) 
	{
		pjsip_evsub_set_mod_data(_Module, pjsua_var.mod.id, NULL);

		pjsip_tx_data *tdata;
		pj_status_t st = pjsip_wg67_initiate(_Module, 0, &tdata);
		if (st == PJ_SUCCESS) 
		{
			pjsua_process_msg_data(tdata, NULL);
			st = pjsip_wg67_send_request(_Module, tdata);
		}

		if (st != PJ_SUCCESS) 
		{
			pjsip_wg67_terminate(_Module, PJ_FALSE);
		}
	}
}
