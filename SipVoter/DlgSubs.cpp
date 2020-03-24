#include "DlgSubs.h"
#include "SipAgent.h"
#include "SipCall.h"
#include "ExtraParamAccId.h"

#undef THIS_FILE
#define THIS_FILE		"DlgSubs.cpp"

pjsip_evsub_user DlgSubs::dlg_callback = 
{
	&DlgSubs::on_state,  
	NULL,
	NULL,
	&DlgSubs::on_rx_notify,
	&DlgSubs::on_client_refresh,
	NULL
};

/**
 * DlgSubs.	...
 * Constructor
 * @param	acc_id				Account del agente que utilizaremos
 * @param	dst_uri				Uri al que enviamos la subscripcion al evento
 * @param	expires. Valor del expires. Puede ser cero para recibir la notificacion en modo polling. Si vale -1 entonces toma el valor por defecto
 * @param	by_proxy. Si true entonces el subscribe se envia a traves del proxy
 * @return	-1 si hay error.
 */
DlgSubs::DlgSubs(pjsua_acc_id acc_id, char *dst_uri, int expires, pj_bool_t by_proxy)
{
	_SubsManager = NULL;
	_Pool = NULL;
	_Dlg = NULL;
	_Module = NULL;
	_dlgsub_cb = NULL;
	_Expires = expires;
	_ByProxy = by_proxy;
	_Acc_id = acc_id;
	if ((strlen(dst_uri)+1) > sizeof(_Dst_uri))
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("DlgSubs: dst_uri demasiado largo");
		return;
	}

	strcpy(_Dst_uri, dst_uri);
	_Dst_uri_pj = pj_str(_Dst_uri);

	_Pool = pjsua_pool_create(NULL, 64, 32);
	if (_Pool == NULL)
	{
		throw PJLibException(__FILE__, PJ_ENOMEM).Msg("DlgSubs: No hay suficiente memoria");
		return;
	}
	if (pj_sem_create(_Pool, NULL, 0, 1, &status_sem) != PJ_SUCCESS)
	{
		throw PJLibException(__FILE__, PJ_ENOMEM).Msg("DlgSubs: No se puede crear status_sem");
		return;
	}
}

DlgSubs::~DlgSubs()
{
	_SubsManager = NULL;
	_dlgsub_cb = NULL;
	if (_Pool != NULL)
	{
		pj_pool_release(_Pool);
	}
}

/**
 * Init.	...
 * inicializa la subscripcion. Si no hay existo lo reintenta.
 * @param dlgsub_cb. Callback para pasar notificaciones al SubsManager
 * @param subsmanager. Puntero del SubsManager.
 * @return	-1 si hay error. 
 */
int DlgSubs::Init(DlgSubs_callback dlgsub_cb, void *subsmanager)
{
	_SubsManager = subsmanager;
	_dlgsub_cb = NULL;
	int tries = 5;
	while (tries > 0)
	{
		if (InicializarSubscripcion() == 0)
		{
			//Se ha activado la subscripcion sin errores
			//Activamos la callback
			_dlgsub_cb = dlgsub_cb;
			return 0;
		}
		tries--;
	}
	PJ_LOG(3,(__FILE__, "ERROR: DlgSubs::Init: No se ha podido realizar la subscripcion a evento dialog despues de %d intentos acc_id %d dst %s", tries, _Acc_id, _Dst_uri)); 
	return -1;
}

/**
 * InicializarSubscripcion.	...
 * Inicializa la subscripcion. 
 * @return	-1 si hay error. 
 */
int DlgSubs::InicializarSubscripcion()
{

	pj_status_t st = PJ_SUCCESS;
	pj_str_t contact;
	st = pjsua_acc_create_uac_contact(_Pool, &contact, _Acc_id, &_Dst_uri_pj, NULL);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: DlgSubs::InicializarSubscripcion: No se puede generar contact para acc %d dst %s", _Acc_id, _Dst_uri_pj.ptr)); 
		return -1;
	}

	pjsua_acc * acc = &pjsua_var.acc[_Acc_id];
	if (!acc->valid)
	{
		PJ_LOG(3,(__FILE__, "ERROR: DlgSubs::InicializarSubscripcion: Account %d no valido", _Acc_id)); 
		return -1;
	}

	st = pjsip_dlg_create_uac(pjsip_ua_instance(), &acc->cfg.id, &contact, &_Dst_uri_pj, NULL, &_Dlg);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: DlgSubs::InicializarSubscripcion: Unable to create dialog  acc_id %d dst %s", _Acc_id, _Dst_uri)); 
		return -1;
	}

	pjsip_dlg_inc_lock(_Dlg);

	st = pjsip_dialog_create_uac( _Dlg, &dlg_callback, PJSIP_EVSUB_NO_EVENT_ID, &_Module);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: DlgSubs::InicializarSubscripcion: Unable to create client %s", _Dst_uri)); 
		_Module = NULL;
		if (_Dlg) pjsip_dlg_dec_lock(_Dlg);
		return -1;
	}

	if (_ByProxy)
	{
		//Si el subscribe va hacia el proxy entonces se agrega la cabecera route
		if (!pj_list_empty(&acc->route_set)) 
		{
			pjsip_dlg_set_route_set(_Dlg, &acc->route_set);
		}
	}

	pjsip_evsub_set_mod_data(_Module, pjsua_var.mod.id, this);

	pjsip_tx_data *tdata;

	st = pjsip_dialog_initiate(_Module, _Expires, &tdata);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: DlgSubs::InicializarSubscripcion: Unable to initiate client %s", _Dst_uri)); 
		if (_Module) pjsip_dialog_terminate(_Module, PJ_FALSE);
		_Module = NULL;
		if (_Dlg) pjsip_dlg_dec_lock(_Dlg);
		return -1;
	}

	//Establecemos aqui la cabecera WG67-version con la version de telefonia. Porque se supone
	//que solo nos subscribimos para telefonia
	//En la funcion OnTxRequest de SipCall.cpp es donde se establece esta cabecera para todos
	//los paquetes SIP, siempre y cuando no se hayan establecido antes, como es este el caso.
	//Se fuerza aqui la cabecera porque el agente que subscribe es el
	//Nodebox, el cual esta definido como un agente radio. 
	SipCall::Wg67VersionSet(tdata, &SipCall::gWG67VersionTelefValue);

	pjsua_process_msg_data(tdata, NULL);

	while (pj_sem_trywait(status_sem) == PJ_SUCCESS);
	st = pjsip_dialog_send_request(_Module, tdata);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: DlgSubs::InicializarSubscripcion: Unable to send initial SUBSCRIBE message %s", _Dst_uri)); 
		if (_Module) pjsip_dialog_terminate(_Module, PJ_FALSE);
		_Module = NULL;
		if (_Dlg) pjsip_dlg_dec_lock(_Dlg);
		return -1;
	}

	if (_Dlg) pjsip_dlg_dec_lock(_Dlg);

	pj_sem_wait_for(status_sem, pjsip_cfg()->tsx.tsx_tout / 2);		//esperamos el tiempo maximo para una transaccion

	if (_Module)
	{
		pjsip_evsub_state status = pjsip_evsub_get_state(_Module);
		
		if (status == PJSIP_EVSUB_STATE_ACCEPTED || status == PJSIP_EVSUB_STATE_ACTIVE)
		{		
			return 0;
		}	

		//No se ha recibido a tiempo la confirmacion de la subscripcion. Terminamos.
		pjsip_dialog_terminate(_Module, PJ_FALSE);
		_Module = NULL;
	}

	PJ_LOG(5,(__FILE__, "ERROR: DlgSubs::InicializarSubscripcion: SUBSCRIBE message is not answered %s", _Dst_uri));

	return -1;
}

/**
 * Init.	...
 * Finaliza la subscripcion
 * @return	-1 si hay error.
 */
int DlgSubs::End()
{
	if (_Module == NULL) return 0;

	_dlgsub_cb = NULL;	//No quiero que se llame a esta callback cuando quiero finalizar la subscripcion.

	pjsip_evsub_state sta = pjsip_evsub_get_state(_Module);

	if (sta == PJSIP_EVSUB_STATE_TERMINATED)
	{		
		_Module = NULL;
		return 0;
	}

	pjsip_evsub_set_mod_data(_Module, pjsua_var.mod.id, NULL);

	pjsip_tx_data *tdata;
	pj_status_t st = pjsip_dialog_initiate(_Module, 0, &tdata);
	if (st != PJ_SUCCESS)
	{
		if (_Module) 
		{
			pjsip_dialog_terminate(_Module, PJ_FALSE);
		}
		_Module = NULL;
		return 0;
	}

	pjsua_process_msg_data(tdata, NULL);
	SipCall::Wg67VersionSet(tdata, &SipCall::gWG67VersionTelefValue);
	while (pj_sem_trywait(status_sem) == PJ_SUCCESS);
	st = pjsip_dialog_send_request(_Module, tdata);
	if (st != PJ_SUCCESS)
	{
		pjsip_dialog_terminate(_Module, PJ_FALSE);
		_Module = NULL;
		return 0;
	}
	else
	{
		//Esperamos a que se haya recibido el 200 ok
		int tries = pjsip_cfg()->tsx.tsx_tout / 40;
		pjsip_evsub_state status = pjsip_evsub_get_state(_Module);
		while (status != PJSIP_EVSUB_STATE_TERMINATED && _Module)
		{
			pj_sem_wait_for(status_sem, 40);
			tries--;
			if (tries == 0) break;
			if (_Module)
			{						
				status = pjsip_evsub_get_state(_Module);
			}
		}
		_Module = NULL;
	}

	return 0;
}

void DlgSubs::on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, 
									pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body)
{
	PJ_LOG(5,(THIS_FILE, "DlgSubs::on_rx_notify sub %p", sub));

	if (SipAgent::Cb.DialogNotifyCb)
	{
		SipAgent::Cb.DialogNotifyCb((const char *) rdata->msg_info.msg->body->data, rdata->msg_info.msg->body->len);
	}


}

void DlgSubs::on_state(pjsip_evsub *sub, pjsip_event *event)
{
	PJ_UNUSED_ARG(event);

	pjsip_evsub_state state;
	pj_bool_t terminar = PJ_TRUE;

	DlgSubs * dlgsub = reinterpret_cast<DlgSubs*>(pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id));

	if (dlgsub) 
	{
		PJ_LOG(4,(THIS_FILE, "DLG-IN subscription to %.*s is %s",
			dlgsub->_Dst_uri_pj.slen, dlgsub->_Dst_uri_pj.ptr, pjsip_evsub_get_state_name(sub)));

		state = pjsip_evsub_get_state(sub);
		switch (state)
		{
		case PJSIP_EVSUB_STATE_NULL:
			PJ_LOG(5,(THIS_FILE, "DlgSubs::on_state PJSIP_EVSUB_STATE_NULL dst %s", dlgsub->_Dst_uri_pj.ptr));
			break;
		case PJSIP_EVSUB_STATE_SENT:
			PJ_LOG(5,(THIS_FILE, "DlgSubs::on_state PJSIP_EVSUB_STATE_SENT dst %s", dlgsub->_Dst_uri_pj.ptr));
			break;
		case PJSIP_EVSUB_STATE_ACCEPTED:
			PJ_LOG(5,(THIS_FILE, "DlgSubs::on_state PJSIP_EVSUB_STATE_ACCEPTED dst %s", dlgsub->_Dst_uri_pj.ptr));
			pj_sem_post(dlgsub->status_sem);
			break;
		case PJSIP_EVSUB_STATE_PENDING:
			PJ_LOG(5,(THIS_FILE, "DlgSubs::on_state PJSIP_EVSUB_STATE_PENDING dst %s", dlgsub->_Dst_uri_pj.ptr));
			break;
		case PJSIP_EVSUB_STATE_ACTIVE:
			PJ_LOG(5,(THIS_FILE, "DlgSubs::on_state PJSIP_EVSUB_STATE_ACTIVE dst %s", dlgsub->_Dst_uri_pj.ptr));
			pj_sem_post(dlgsub->status_sem);
			break;
		case PJSIP_EVSUB_STATE_TERMINATED:
			PJ_LOG(5,(THIS_FILE, "DlgSubs::on_state PJSIP_EVSUB_STATE_TERMINATED dst %s", dlgsub->_Dst_uri_pj.ptr));

			//Ha cambiado el estado de la subscripcion
			dlgsub->_Module = NULL;
			pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);	

			pj_sem_post(dlgsub->status_sem);
			break;
		case PJSIP_EVSUB_STATE_UNKNOWN:
			PJ_LOG(5,(THIS_FILE, "DlgSubs::on_state PJSIP_EVSUB_STATE_UNKNOWN dst %s", dlgsub->_Dst_uri_pj.ptr));
			break;
		}

		if (dlgsub->_dlgsub_cb != NULL)
		{
			dlgsub->_dlgsub_cb((void *) dlgsub);
		}
	}
}


/**
 * on_client_refresh.	...
 * Funcion callback que se llama cada vez que hay que refrescar la subscripcion.
 * Se utiliza para poder agregar la cabecera WG67-version que corresponde
 */
void DlgSubs::on_client_refresh(pjsip_evsub *sub)
{
	pj_status_t status;
	pjsip_tx_data *tdata;

	status = pjsip_evsub_initiate(sub, &pjsip_subscribe_method, -1, &tdata);
	if (status == PJ_SUCCESS) {

		//Establecemos aqui la cabecera WG67-version con la version de telefonia. Porque se supone
		//que solo nos subscribimos en el servidor de presencia para telefonia
		//En la funcion OnTxRequest de SipCall.cpp es donde se establece esta cabecera para todos
		//los paquetes SIP, siempre y cuando no se hayan establecido antes, como es este el caso.
		//Se fuerza aqui la cabecera porque el agente que subscribe al servidor es el
		//Nodebox, el cual esta definido como un agente radio. 
		SipCall::Wg67VersionSet(tdata, &SipCall::gWG67VersionTelefValue);

	    pjsip_evsub_send_request(sub, tdata);
    }
}


/********************************************************************************************/
/* A partir de aqui se sefiere a cuando funciona como servidor de subscripcion*/
/********************************************************************************************/

pjsip_evsub_user DlgSubs::_DlgSrvCb = 
{
	&DlgSubs::OnDlgSrvStateChanged,  
	&DlgSubs::OnDlgSrvTsxChanged,
	&DlgSubs::OnDlgSrvRxRefresh,
	NULL,
	NULL,
	&DlgSubs::OnDlgSrvTimeout
};


void DlgSubs::OnDlgSrvStateChanged(pjsip_evsub *sub, pjsip_event *event)
{
	pjsip_evsub_state state = pjsip_evsub_get_state(sub);
	PJ_LOG(5, ("SipAgent.cpp", "Dialog server subscription is %s sub %p", pjsip_evsub_get_state_name(sub), sub));	
	if (state == PJSIP_EVSUB_STATE_TERMINATED) 
	{	
		/*Al terminar la subscripcion al evento de dialogo, lo eliminamos de la lista del account correspondiente*/
		ExtraParamAccId::Del_subMod(sub);
		pjsip_evsub_set_user_data(sub, NULL);				
	} 
}

void DlgSubs::OnDlgSrvTsxChanged(pjsip_evsub *sub, pjsip_transaction *tsx, pjsip_event *event)
{
	pjsip_evsub_state state = pjsip_evsub_get_state(sub);
	PJ_LOG(5, ("SipAgent.cpp", "OnDlgSrvTsxChanged %s sub %p", pjsip_evsub_get_state_name(sub), sub));
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED && (tsx->role == PJSIP_ROLE_UAS) && (pjsip_method_cmp(&tsx->method, &pjsip_subscribe_method) == 0)
		&& state != PJSIP_EVSUB_STATE_NULL && state != PJSIP_EVSUB_STATE_SENT)
	{
		ExtraParamAccId::subs_user_data *sub_user_data = (ExtraParamAccId::subs_user_data *) pjsip_evsub_get_user_data(sub);
		if (sub_user_data != NULL && sub_user_data->send_notify_at_tsx_terminate)
		{
			sub_user_data->send_notify_at_tsx_terminate = PJ_FALSE;
			if (state != PJSIP_EVSUB_STATE_ACTIVE)
			{
				//En este caso el expires del primer subscribe habria llegado a cero. Porque si no entonces el estado seria active
				//Por tanto el ultimo notify (en el caso de que hubiera que enviar varios) llevara el estado terminated a true
				Current_notify_all_dialogs(sub, TERMINATE, WITH_BODY);				
			}
			else
			{
				Current_notify_all_dialogs(sub, !TERMINATE, WITH_BODY);
			}
		}
	}
}

void DlgSubs::OnDlgSrvRxRefresh( pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code,
			   pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body)
{
	/* Implementors MUST send NOTIFY if it implements on_rx_refresh */
	pj_str_t timeout = { "timeout", 7};
	pj_status_t status;

	if (pjsip_evsub_get_state(sub)==PJSIP_EVSUB_STATE_TERMINATED) {
		status = Notify_all_dialogs( sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, &timeout, !WITH_BODY);
	} else {
		status = Current_notify_all_dialogs(sub, !TERMINATE, !WITH_BODY);
	}
}

 void DlgSubs::OnDlgSrvTimeout(pjsip_evsub *sub)
 {
	pj_status_t status;
	pj_str_t reason = { "timeout", 7 };

	status = Notify_all_dialogs(sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, &reason, !WITH_BODY);
 }

/*
* Create and send NOTIFY that reflect current state of all dialogs.
* @param callid. Call id de la llamada a la que wse refiere el dialogo sobre el que se envia la notificacion
* @sub.	objeto de la subscripcion al evento de dialogo
* @param terminate	Si es true indica que enviamos un ultimo notify vacio con estado a terminate
* @param with_body. Includes body in the notify
*/
pj_status_t DlgSubs::Current_notify_all_dialogs( pjsip_evsub *sub, pj_bool_t terminate, pj_bool_t with_body )
{
	pj_status_t sst = PJ_SUCCESS;
	unsigned int i;
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned call_cnt=PJ_ARRAY_SIZE(call_ids);

	sst = pjsua_enum_calls(call_ids, &call_cnt);
	if (sst == PJ_SUCCESS && with_body == PJ_TRUE)
	{
		for (i = 0; i < call_cnt; i++)
		{
			SipCall * call = (SipCall*)pjsua_call_get_user_data(call_ids[i]);
			if (!call) continue;
			if (call->_Info.Type == CORESIP_CALL_IA)
			{
				//Los eventos de dialogo de llamada del tipo IA no se envian
				continue;
			}

			pjsip_dialog_current_notify( call_ids[i], sub, with_body );
		}
	}

	if (sst != PJ_SUCCESS || with_body == PJ_FALSE || call_cnt == 0 || terminate == PJ_TRUE)
	{
		//Si no hay dialogos activos enviamos un notify sin body

		if (terminate == PJ_TRUE)
		{
			//Si state es terminate, enviamos un notify vacio con terminate
			pjsip_evsub_set_state(sub, PJSIP_EVSUB_STATE_TERMINATED);
		}
		pjsip_dialog_current_notify( PJSUA_INVALID_ID, sub, !WITH_BODY );
	}

	return sst;
}

/*
* Create NOTIFY of all dialogs
* @param callid. Call id de la llamada a la que wse refiere el dialogo sobre el que se envia la notificacion
* @sub.	objeto de la subscripcion al evento de dialogo
....
* @param with_body. Includes body in the notify
*/
pj_status_t DlgSubs::Notify_all_dialogs( pjsip_evsub *sub,
												  pjsip_evsub_state state,
												  const pj_str_t *state_str,
												  const pj_str_t *reason,
												  pj_bool_t with_body)
{
	pj_status_t sst = PJ_SUCCESS;
	unsigned int i;
	pjsua_call_id call_ids[PJSUA_MAX_CALLS];
	unsigned call_cnt=PJ_ARRAY_SIZE(call_ids);
		
	sst = pjsua_enum_calls(call_ids, &call_cnt);
	if (sst == PJ_SUCCESS && with_body == PJ_TRUE)
	{
		for (i = 0; i < call_cnt; i++)
		{
			SipCall * call = (SipCall*)pjsua_call_get_user_data(call_ids[i]);
			if (!call) continue;
			if (call->_Info.Type == CORESIP_CALL_IA)
			{
				//Los eventos de dialogo de llamada del tipo IA no se envian
				continue;
			}


			//Si state es terminated, no podemos enviar todos los notify con ese estado, porque si no el primero
			//finaliza la subscripcion en el cliente y el resto serian rechazados
			//Por ello solo el ultimo notify que enviaremos vacío ira en estado terminate

			if (state == PJSIP_EVSUB_STATE_TERMINATED)
			{
				pjsip_dialog_notify( call_ids[i], sub, PJSIP_EVSUB_STATE_ACTIVE, NULL, reason, with_body );
			}
			else
			{
				pjsip_dialog_notify( call_ids[i], sub, state, state_str, reason, with_body);
			}
		}
	}

	if (sst != PJ_SUCCESS || with_body == PJ_FALSE || call_cnt == 0 || state == PJSIP_EVSUB_STATE_TERMINATED)
	{
		//Si no se requiere body
		//Si no hay dialogos activos 
		//O si el estado es terminated enviamos un notify vacio

		pjsip_dialog_notify( PJSUA_INVALID_ID, sub, state, state_str, reason, !WITH_BODY);
	}

	return sst;
}







