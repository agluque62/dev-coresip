/*
En este modulo se implementa la clase que gestina una subscripcion al evento de conferencia
*/
#include "ConfSubs.h"
#include "SipCall.h"
#include "SipAgent.h"
#include "ExtraParamAccId.h"

#undef THIS_FILE
#define THIS_FILE		"ConfSubs.cpp"

pjsip_evsub_user ConfSubs::conf_callback = 
{
	&ConfSubs::conf_on_state,  
	NULL,
	NULL,
	&ConfSubs::conf_on_rx_notify,
	&ConfSubs::conf_on_client_refresh,
	NULL
};

/**
 * ConfSubs.	...
 * Constructor
 * @param	acc_id				Account del agente que utilizaremos
 * @param	dst_uri				Uri al que enviamos la subscripcion al evento
 * @param	expires. Valor del expires. Si vale -1 entonces toma el valor por defecto
 * @param	by_proxy. Si true entonces el subscribe se envia a traves del proxy
 * @return	-1 si hay error.
 */
ConfSubs::ConfSubs(pjsua_acc_id acc_id, char *dst_uri, int expires, pj_bool_t by_proxy)
{
	_confsub_cb = NULL;
	_Pool = NULL;
	_Dlg = NULL;
	_Module = NULL;
	_SubsManager = NULL;
	_Expires = expires;
	_ByProxy = by_proxy;

	_Acc_id = acc_id;
	if ((strlen(dst_uri)+1) > sizeof(_Dst_uri))
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("ConfSubs: dst_uri demasiado largo");
		return;
	}

	strcpy(_Dst_uri, dst_uri);
	_Dst_uri_pj = pj_str(_Dst_uri);

	_Pool = pjsua_pool_create(NULL, 64, 32);
	if (_Pool == NULL)
	{
		throw PJLibException(__FILE__, PJ_ENOMEM).Msg("ConfSubs: No hay suficiente memoria");
		return;
	}

	if (pj_sem_create(_Pool, NULL, 0, 1, &status_sem) != PJ_SUCCESS)
	{
		throw PJLibException(__FILE__, PJ_ENOMEM).Msg("ConfSubs: No se puede crear status_sem");
		return;
	}
}


ConfSubs::~ConfSubs()
{
	_confsub_cb = NULL;
	_SubsManager = NULL;
	if (_Pool != NULL)
	{
		pj_pool_release(_Pool);
	}
}

/**
 * Init.	...
 * inicializa la subscripcion. Si no hay existo lo reintenta.
 * @param confsub_cb. Callback para pasar notificaciones al SubsManager
 * @param subsmanager. Puntero del SubsManager.
 * @return	-1 si hay error. 
 */
int ConfSubs::Init(ConfSubs_callback confsub_cb, void *subsmanager)
{
	_confsub_cb = NULL;
	_SubsManager = subsmanager;
	int tries = 5;
	while (tries > 0)
	{
		if (InicializarSubscripcion() == 0)
		{
			//Se ha activado la subscripcion sin errores
			//Activamos la callback
			_confsub_cb = confsub_cb;
			return 0;
		}
		tries--;
	}
	PJ_LOG(3,(__FILE__, "ERROR: ConfSubs::Init: No se ha podido realizar la subscripcion a conf despues de %d intentos acc_id %d dst %s", tries, _Acc_id, _Dst_uri)); 
	return -1;
}

/**
 * InicializarSubscripcion.	...
 * Inicializa la subscripcion
 * @return	-1 si hay error. 
 */
int ConfSubs::InicializarSubscripcion()
{
	/*
	if (_Dlg != NULL)
	{
		PJ_LOG(3,(__FILE__, "ERROR: ConfSubs::InicializarSubscripcion: Ya hay generado un dialogo. Solo se puede invocar Init una vez acc_id %d dst %s", _Acc_id, _Dst_uri)); 
		return -1;
	}
	*/

	pj_status_t st = PJ_SUCCESS;
	pj_str_t contact;
	st = pjsua_acc_create_uac_contact(_Pool, &contact, _Acc_id, &_Dst_uri_pj, NULL);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: ConfSubs::InicializarSubscripcion: No se puede generar contact para acc %d dst %s", _Acc_id, _Dst_uri_pj.ptr)); 
		return -1;
	}

	pjsua_acc * acc = &pjsua_var.acc[_Acc_id];
	if (!acc->valid)
	{
		PJ_LOG(3,(__FILE__, "ERROR: ConfSubs::InicializarSubscripcion: Account %d no valido", _Acc_id)); 
		return -1;
	}

	st = pjsip_dlg_create_uac(pjsip_ua_instance(), &acc->cfg.id, &contact, &_Dst_uri_pj, NULL, &_Dlg);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: ConfSubs::InicializarSubscripcion: Unable to create dialog  acc_id %d dst %s", _Acc_id, _Dst_uri)); 
		return -1;
	}

	pjsip_dlg_inc_lock(_Dlg);

	st = pjsip_conf_create_uac(_Dlg, &conf_callback, PJSIP_EVSUB_NO_EVENT_ID, &_Module);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: ConfSubs::InicializarSubscripcion: Unable to create client %s", _Dst_uri)); 
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

	st = pjsip_conf_initiate(_Module, _Expires, &tdata);

	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: ConfSubs::InicializarSubscripcion: Unable to initiate client %s", _Dst_uri)); 
		if (_Module) pjsip_conf_terminate(_Module, PJ_FALSE);
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
	st = pjsip_conf_send_request(_Module, tdata);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: ConfSubs::InicializarSubscripcion: Unable to send initial SUBSCRIBE message %s", _Dst_uri)); 
		if (_Module) pjsip_conf_terminate(_Module, PJ_FALSE);
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
		pjsip_conf_terminate(_Module, PJ_FALSE);
		_Module = NULL;
	}

	PJ_LOG(5,(__FILE__, "ERROR: ConfSubs::InicializarSubscripcion: SUBSCRIBE message is not answered %s", _Dst_uri)); 

	return -1;
}

/**
 * Init.	...
 * Finaliza la subscripcion
 * @return	-1 si hay error.
 */
int ConfSubs::End()
{
	if (_Module == NULL) return 0;

	_confsub_cb = NULL;	//No quiero que se llame a esta callback cuando quiero finalizar la subscripcion.

	pjsip_evsub_state sta = pjsip_evsub_get_state(_Module);

	if (sta == PJSIP_EVSUB_STATE_TERMINATED)
	{		
		_Module = NULL;
		return 0;
	}

	pjsip_evsub_set_mod_data(_Module, pjsua_var.mod.id, NULL);

	pjsip_tx_data *tdata;
	pj_status_t st = pjsip_conf_initiate(_Module, 0, &tdata);
	if (st != PJ_SUCCESS)
	{
		if (_Module) 
		{
			pjsip_conf_terminate(_Module, PJ_FALSE);
		}
		_Module = NULL;
		return 0;
	}

	pjsua_process_msg_data(tdata, NULL);
	SipCall::Wg67VersionSet(tdata, &SipCall::gWG67VersionTelefValue);
	while (pj_sem_trywait(status_sem) == PJ_SUCCESS);
	st = pjsip_conf_send_request(_Module, tdata);
	if (st != PJ_SUCCESS)
	{
		pjsip_conf_terminate(_Module, PJ_FALSE);
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

void ConfSubs::conf_on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, 
									pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body)
{
	ConfSubs * conf = reinterpret_cast<ConfSubs*>(pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id));
	if (conf && SipAgent::Cb.ConfInfoCb) 
	{	
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

			SipAgent::Cb.ConfInfoCb(conf->_Acc_id | CORESIP_ACC_ID, &info, from_urich, size);
		}
	}
}

void ConfSubs::conf_on_state(pjsip_evsub *sub, pjsip_event *event)
{
	PJ_UNUSED_ARG(event);

	pjsip_evsub_state state;
	pj_bool_t terminar = PJ_TRUE;

	ConfSubs * conf = reinterpret_cast<ConfSubs*>(pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id));

	if (conf) 
	{
		PJ_LOG(4,(THIS_FILE, "CONF-IN subscription to %.*s is %s",
			conf->_Dst_uri_pj.slen, conf->_Dst_uri_pj.ptr, pjsip_evsub_get_state_name(sub)));

		state = pjsip_evsub_get_state(sub);
		switch (state)
		{
		case PJSIP_EVSUB_STATE_NULL:
			PJ_LOG(5,(THIS_FILE, "ConfSubs::conf_on_state PJSIP_EVSUB_STATE_NULL dst %s", conf->_Dst_uri_pj.ptr));
			break;
		case PJSIP_EVSUB_STATE_SENT:
			PJ_LOG(5,(THIS_FILE, "ConfSubs::conf_on_state PJSIP_EVSUB_STATE_SENT dst %s", conf->_Dst_uri_pj.ptr));
			break;
		case PJSIP_EVSUB_STATE_ACCEPTED:
			PJ_LOG(5,(THIS_FILE, "ConfSubs::conf_on_state PJSIP_EVSUB_STATE_ACCEPTED dst %s", conf->_Dst_uri_pj.ptr));
			pj_sem_post(conf->status_sem);
			break;
		case PJSIP_EVSUB_STATE_PENDING:
			PJ_LOG(5,(THIS_FILE, "ConfSubs::conf_on_state PJSIP_EVSUB_STATE_PENDING dst %s", conf->_Dst_uri_pj.ptr));
			break;
		case PJSIP_EVSUB_STATE_ACTIVE:
			PJ_LOG(5,(THIS_FILE, "ConfSubs::conf_on_state PJSIP_EVSUB_STATE_ACTIVE dst %s", conf->_Dst_uri_pj.ptr));
			pj_sem_post(conf->status_sem);
			break;
		case PJSIP_EVSUB_STATE_TERMINATED:
			PJ_LOG(5,(THIS_FILE, "ConfSubs::conf_on_state PJSIP_EVSUB_STATE_TERMINATED dst %s", conf->_Dst_uri_pj.ptr));

			//Ha cambiado el estado de la subscripcion
			conf->_Module = NULL;
			pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);	

			pj_sem_post(conf->status_sem);
			break;
		case PJSIP_EVSUB_STATE_UNKNOWN:
			PJ_LOG(5,(THIS_FILE, "ConfSubs::conf_on_state PJSIP_EVSUB_STATE_UNKNOWN dst %s", conf->_Dst_uri_pj.ptr));
			break;
		}

		if (conf->_confsub_cb != NULL)
		{
			conf->_confsub_cb((void *) conf);
		}
	}
}


/**
 * conf_on_client_refresh.	...
 * Funcion callback que se llama cada vez que hay que refrescar la subscripcion.
 * Se utiliza para poder agregar la cabecera WG67-version que corresponde
 */
void ConfSubs::conf_on_client_refresh(pjsip_evsub *sub)
{
	pj_status_t status;
	pjsip_tx_data *tdata;

	status = pjsip_conf_initiate(sub, -1, &tdata);
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

pjsip_evsub_user ConfSubs::_ConfSrvCb = 
{
	&ConfSubs::OnConfSrvStateChanged,  
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

void ConfSubs::OnConfSrvStateChanged(pjsip_evsub *sub, pjsip_event *event)
{
	PJ_LOG(5, ("SipAgent.cpp", "Conference server subscription is %s sub %p", pjsip_evsub_get_state_name(sub), sub));
	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) 
	{	
		/*Al terminar la subscripcion al evento de conferencia, lo eliminamos de la lista del account correspondiente*/
		ExtraParamAccId::Del_subMod(sub);
		pjsip_evsub_set_user_data(sub, NULL);		
	} 
}