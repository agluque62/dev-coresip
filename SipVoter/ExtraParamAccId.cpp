/**
 * @file ExtraParamAccId.cpp
 *
 * Define la clase del objeto que se asigna como user data a un account id
 */
#include "ExtraParamAccId.h"
#include "dlgsub.h"

ExtraParamAccId::ExtraParamAccId()
{
	rdAccount = PJ_FALSE;
	TipoGrsFlags = CORESIP_CALL_NINGUNO;	
	_Pool = pjsua_pool_create(NULL, 64, 32);
	if (_Pool == NULL)
	{
		throw PJLibException(__FILE__, PJ_ENOMEM).Msg("ExtraParamAccId: No hay suficiente memoria");
		return;
	}

	_subModlist_mutex = NULL;
	pj_status_t st = pj_mutex_create_simple(_Pool, NULL, &_subModlist_mutex); 
	if (st != PJ_SUCCESS)
	{
		throw PJLibException(__FILE__, PJ_ENOMEM).Msg("ExtraParamAccId: No se puede crear _subModlist_mutex");
		return;
	}
}

ExtraParamAccId::~ExtraParamAccId()
{
	if (_subModlist_mutex != NULL)
	{
		pj_mutex_destroy(_subModlist_mutex);
	}
	if (_Pool != NULL)
	{
		pj_pool_release(_Pool);
	}
}

/**
 * Add_subMod: Agrega a la lista de subscripciones correspondientes a un account
 * @param	sub	Subscripcion que agrega
 * @return	0 si no hay error
 */
int ExtraParamAccId::Add_subMod(pjsip_evsub *sub)
{
	subs_user_data *sub_user_data = (subs_user_data *) pjsip_evsub_get_user_data(sub);
	if (sub_user_data == NULL)
	{
		PJ_LOG(3, ("SipAgent.cpp", "ERROR: Add_subMod evsub invalido"));
		return -1;
	}
	pjsua_acc_id accid = sub_user_data->accid;
	if (accid == PJSUA_INVALID_ID)
	{
		PJ_LOG(3, ("SipAgent.cpp", "ERROR: Add_subMod evsub invalido"));
		return -1;
	}

	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(accid);
	if (extraParamAccCfg != NULL)
	{
		pj_mutex_lock(extraParamAccCfg->_subModlist_mutex);
		extraParamAccCfg->_subModlist.remove(sub);		//Lo quito por si ya estuviera y me aseguro de que solo hay uno
		extraParamAccCfg->_subModlist.push_back(sub);	//Agrego el nuevo elemento
		pj_mutex_unlock(extraParamAccCfg->_subModlist_mutex);
	}
	return 0;
}

/**
 * Del_subMod: Quita de la lista de subscripciones correspondientes a un account
 * @param	sub	Subscripcion que quita
 * @return	0 si no hay error
 */
int ExtraParamAccId::Del_subMod(pjsip_evsub *sub)
{
	subs_user_data *sub_user_data = (subs_user_data *) pjsip_evsub_get_user_data(sub);
	if (sub_user_data == NULL)
	{
		PJ_LOG(5, ("ExtraParamAccId", "WARNING: Del_subMod evsub parece que ya ha sido borrada"));
		return -1;
	}
	pjsua_acc_id accid = sub_user_data->accid;
	if (accid == PJSUA_INVALID_ID)
	{
		PJ_LOG(5, ("ExtraParamAccId", "WARNING: Del_subMod evsub parece que ya ha sido borrada"));
		return -1;
	}

	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(accid);
	if (extraParamAccCfg != NULL)
	{
		pj_mutex_lock(extraParamAccCfg->_subModlist_mutex);
		extraParamAccCfg->_subModlist.remove(sub);				//Lo quitamos de la lista de subscripciones activas
		pj_mutex_unlock(extraParamAccCfg->_subModlist_mutex);
	}
	return 0;
}

/**
 * Get_subMod: Retorna el objeto pjsip_evsub para un contact concreto y un tipo de evento 
 * @param	accid	Account id			Account id
 * @param	remote_uri	Uri del contact
 * @return	NULL si no lo encuentra
 */
pjsip_evsub * ExtraParamAccId::Get_subMod(pjsua_acc_id accid, pjsip_uri *remote_uri, pj_str_t *event_type)
{
	if (accid<0 || accid>=(int)PJ_ARRAY_SIZE(pjsua_var.acc))
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Get_subMod:", "accid %d no valido", accid);		
	}

	pjsip_evsub * ret = NULL;

	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(accid);
	if (extraParamAccCfg == NULL) return ret;

	pj_mutex_lock(extraParamAccCfg->_subModlist_mutex);
	std::list<pjsip_evsub *, std::allocator<pjsip_evsub *>>::iterator it;
	it = extraParamAccCfg->_subModlist.begin();
	for (it = extraParamAccCfg->_subModlist.begin(); it != extraParamAccCfg->_subModlist.end(); it++)
	{
		pjsip_evsub *sub = *it;

		if (sub)
		{
			pjsip_dialog* dlg = pjsip_evsub_get_dlg(sub);			
			if (dlg != NULL)
			{
				pjsip_dlg_inc_lock(dlg);
				pjsip_event_hdr *eventhdr = (pjsip_event_hdr *) pjsip_evsub_get_event_hdr(sub);		
				if (eventhdr != NULL)
				{
					if ((pjsip_uri_cmp(PJSIP_URI_IN_CONTACT_HDR, dlg->remote.contact->uri, remote_uri) == PJ_SUCCESS) &&
						(pj_strcmp(event_type, &eventhdr->event_type) == 0))
					{
						ret = sub;
						pjsip_dlg_dec_lock(dlg);
						break;
					}
				}
				pjsip_dlg_dec_lock(dlg);
			}
		}
	}
	pj_mutex_unlock(extraParamAccCfg->_subModlist_mutex);

	return ret;
}

/**
 * Add_DeletedsubModlist: Agrega un elemento a _DeletedConfsubModlist
 * @param	accid	Account id			Account id
 * @param	remote_uri	Uri del from
 * @param	remote_from_tag	Uri del from
 * Retorn -1 si hay error
 */
int ExtraParamAccId::Add_DeletedsubModlist(pjsua_acc_id accid, pjsip_uri *remote_uri, pj_str_t *remote_from_tag)
{
	if (accid<0 || accid>=(int)PJ_ARRAY_SIZE(pjsua_var.acc))
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("Add_DeletedsubModlist:", "accid %d no valido", accid);		
		return -1;
	}

	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(accid);
	if (extraParamAccCfg != NULL)
	{
		pj_bool_t agregar_a_DeletedsubModlist = PJ_TRUE;
		from_user_subs *fuser = (from_user_subs *) malloc(sizeof(from_user_subs));
		if (fuser == NULL)
		{
			PJ_LOG(3, ("ExtraParamAccId", "ERROR: Add_DeletedsubModlist No hay memoria"));
			agregar_a_DeletedsubModlist = PJ_FALSE;
		}
		else
		{
			pjsip_sip_uri * from_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(remote_uri);
			if (from_uri == NULL) agregar_a_DeletedsubModlist = PJ_FALSE;
			else
			{
				if (IsInDeletedList(accid, remote_uri, remote_from_tag))
				{
					//Ya esta en la lista de borrados. No lo agregamos
					agregar_a_DeletedsubModlist = PJ_FALSE;
				}
				else if ((from_uri->user.slen > ((sizeof(fuser->user)/sizeof(char)) - 1)) ||
					(from_uri->host.slen > ((sizeof(fuser->host)/sizeof(char)) - 1)) ||
					(remote_from_tag->slen > ((sizeof(fuser->tag)/sizeof(char)) - 1)))
				{
					agregar_a_DeletedsubModlist = PJ_FALSE;
				}
				else
				{
					strncpy(fuser->user, from_uri->user.ptr, from_uri->user.slen);
					fuser->user[from_uri->user.slen] = '\0';
					strncpy(fuser->host, from_uri->host.ptr, from_uri->host.slen);
					fuser->user[from_uri->host.slen] = '\0';
					strncpy(fuser->tag, remote_from_tag->ptr, remote_from_tag->slen);
					fuser->user[remote_from_tag->slen] = '\0';
				}
			}
		}

		if (agregar_a_DeletedsubModlist)
		{
			//Lo ponemos en la lista de subscripciones borradas
			pj_mutex_lock(extraParamAccCfg->_subModlist_mutex);
			if (extraParamAccCfg->_DeletedsubModlist.size() == MAX_DeletedsubModlist_size)
			{
				//La lista está llena. Quitamos el elemento mas antiguo
				from_user_subs *fuser_tmp = extraParamAccCfg->_DeletedsubModlist.front();
				free(fuser_tmp);
				extraParamAccCfg->_DeletedsubModlist.pop_front();
			}
			//Agregamos un nuevo elemento a la lista de los borrados
			extraParamAccCfg->_DeletedsubModlist.push_back(fuser);	
			pj_mutex_unlock(extraParamAccCfg->_subModlist_mutex);
		}
		else if (fuser != NULL)
		{
			free(fuser);
		}
	}
	return 0;
}

/**
 * IsInDeletedList: Busca en la lista de subscripciones borradas una from uri y un from tag.
 * Es posible que nos llegue una solicitud de subscripcion rezagada de una que ya se había descartado
 * @param	accid	Account id			Account id
 * @param	remote_uri	Uri del from
 * @param	remote_from_tag	Uri del from
 * @return	Retorna true si esta en la lista
 */
pj_bool_t ExtraParamAccId::IsInDeletedList(pjsua_acc_id accid, pjsip_uri *remote_uri, pj_str_t *remote_from_tag)
{
	if (accid<0 || accid>=(int)PJ_ARRAY_SIZE(pjsua_var.acc))
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("SendConfInfoFromAcc:", "accid %d no valido", accid);		
	}

	pj_bool_t ret = PJ_FALSE;

	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(accid);
	if (extraParamAccCfg == NULL) return ret;

	pjsip_sip_uri * from_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(remote_uri);
	if (from_uri == NULL) return ret;

	pj_mutex_lock(extraParamAccCfg->_subModlist_mutex);
	std::list<from_user_subs *, std::allocator<from_user_subs *>>::iterator it;
	it = extraParamAccCfg->_DeletedsubModlist.begin();
	for (it = extraParamAccCfg->_DeletedsubModlist.begin(); it != extraParamAccCfg->_DeletedsubModlist.end(); it++)
	{
		from_user_subs *fuser = *it;
		if (fuser)
		{
			if ((strncmp(fuser->user, from_uri->user.ptr, from_uri->user.slen) == 0) &&
				(strncmp(fuser->host, from_uri->host.ptr, from_uri->host.slen) == 0) &&
				(strncmp(fuser->tag, remote_from_tag->ptr, remote_from_tag->slen) == 0))
			{
				ret = PJ_TRUE;
				break;
			}
		}
	}
	pj_mutex_unlock(extraParamAccCfg->_subModlist_mutex);

	return ret;
}

/**
 * SendConfInfoFromAcc: Envia Notify con la info de la conferencia a todas las subscripciones al evento
 *						de conferencia que tiene un account
 * @param	accid	Account id
 * @param	conf	Info de de conferencia
 * @return	0 si no hay error
 */
void ExtraParamAccId::SendConfInfoFromAcc(pjsua_acc_id accid, const CORESIP_ConfInfo * conf)
{
	if (accid<0 || accid>=(int)PJ_ARRAY_SIZE(pjsua_var.acc))
	{
		throw PJLibException(__FILE__, PJ_EINVAL).Msg("SendConfInfoFromAcc:", "accid %d no valido", accid);		
	}

	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(accid);
	if (extraParamAccCfg == NULL) return;

	pj_mutex_lock(extraParamAccCfg->_subModlist_mutex);
	std::list<pjsip_evsub *, std::allocator<pjsip_evsub *>>::iterator it;
	it = extraParamAccCfg->_subModlist.begin();
	for (it = extraParamAccCfg->_subModlist.begin(); it != extraParamAccCfg->_subModlist.end(); it++)
	{
		pjsip_evsub *confsub = *it;
		if (confsub)
		{
			pjsip_event_hdr *eventhdr = (pjsip_event_hdr *) pjsip_evsub_get_event_hdr(confsub);		
			if (eventhdr)
			{
				pj_str_t STR_CONFERENCE = { "conference", 10 };
				if (pj_strcmp(&eventhdr->event_type, &STR_CONFERENCE) == 0)
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

					pj_status_t st = pjsip_conf_set_status(confsub, &info);
					if (st == PJ_SUCCESS) 
					{
						pjsip_tx_data * tdata;
						st = pjsip_conf_current_notify(confsub, &tdata);
						if (st == PJ_SUCCESS)
						{
							PJ_LOG(5, ("sipcall.cpp", "NOTIFY CONF: Envia conference NOTIFY %s", pjsip_evsub_get_state_name(confsub)));
							st = pjsip_conf_send_request(confsub, tdata);
						}
					}
				}
			}
		}
	}
	pj_mutex_unlock(extraParamAccCfg->_subModlist_mutex);
}

/**
 * SendDialogNotifyFromAcc: Envia el notify del evento de dialogo a todas las subscripciones activas en un account
 * @param	callid. Llamada cuyo evento de dialogo hay que enviar a todas las subscripciones
 * @param with_body. Includes body in the notify
 * @return	0 si no hay error
 */
void ExtraParamAccId::SendDialogNotifyFromAcc(pjsua_call_id call_id, pj_bool_t with_body)
{
	pjsua_call_info callInfo;
	if (pjsua_call_get_info(call_id, &callInfo) != PJ_SUCCESS)
	{	
		return;
	}

	pjsua_acc_id accid = callInfo.acc_id;

	ExtraParamAccId *extraParamAccCfg = (ExtraParamAccId *) pjsua_acc_get_user_data(accid);
	if (extraParamAccCfg == NULL) return;

	pj_mutex_lock(extraParamAccCfg->_subModlist_mutex);
	std::list<pjsip_evsub *, std::allocator<pjsip_evsub *>>::iterator it;
	it = extraParamAccCfg->_subModlist.begin();
	for (it = extraParamAccCfg->_subModlist.begin(); it != extraParamAccCfg->_subModlist.end(); it++)
	{
		pjsip_evsub *dlgsub = *it;
		if (dlgsub)
		{
			pjsip_event_hdr *eventhdr = (pjsip_event_hdr *) pjsip_evsub_get_event_hdr(dlgsub);		
			if (eventhdr)
			{
				pj_str_t STR_DIALOG = { "dialog", 6 };
				if (pj_strcmp(&eventhdr->event_type, &STR_DIALOG) == 0)
				{
					pjsip_dialog_current_notify(call_id, dlgsub, with_body);
				}
			}
		}
	}
	pj_mutex_unlock(extraParamAccCfg->_subModlist_mutex);
}