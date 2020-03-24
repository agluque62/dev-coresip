/* PresSubs.cpp
 * Gestiona las subscripciones individualmente. Inicia la subscripción y contiene las callback para cada subscripcion
 */

#include "Global.h"
#include "PresSubs.h"
#include "exceptions.h"
#include "SipCall.h"

#undef THIS_FILE
#define THIS_FILE		"PresSubs.cpp"

void PresSubs::presence_on_state(pjsip_evsub *sub, pjsip_event *event)
{
	PJ_UNUSED_ARG(event);

	pjsip_evsub_state state;
	pj_bool_t terminar = PJ_TRUE;

	PresSubs * pres = reinterpret_cast<PresSubs*>(pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id));

	if (pres) 
	{
		PJ_LOG(4,(THIS_FILE, "PRES-IN subscription to %.*s is %s",
			pres->_Dst.slen, pres->_Dst.ptr, pjsip_evsub_get_state_name(sub)));

		state = pjsip_evsub_get_state(sub);
		switch (state)
		{
		case PJSIP_EVSUB_STATE_NULL:
			PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_state PJSIP_EVSUB_STATE_NULL dst %s", pres->_Dst.ptr));
			break;
		case PJSIP_EVSUB_STATE_SENT:
			PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_state PJSIP_EVSUB_STATE_SENT dst %s", pres->_Dst.ptr));
			break;
		case PJSIP_EVSUB_STATE_ACCEPTED:
			PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_state PJSIP_EVSUB_STATE_ACCEPTED dst %s", pres->_Dst.ptr));
			break;
		case PJSIP_EVSUB_STATE_PENDING:
			PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_state PJSIP_EVSUB_STATE_PENDING dst %s", pres->_Dst.ptr));
			break;
		case PJSIP_EVSUB_STATE_ACTIVE:
			PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_state PJSIP_EVSUB_STATE_ACTIVE dst %s", pres->_Dst.ptr));
			pres->nSubsTries = 0;
			if (pres->_SubscriptionStatus != ACTIVADA)
			{
				//Ha cambiado el estado de la subscripcion
				pres->_SubscriptionStatus = ACTIVADA;
				if (pres->_PresSubs_callback) (pres->_PresSubs_callback)(pres->_Dst.ptr, pres->_SubscriptionStatus, pres->_PresenceStatus, pres->_Data);				
			}
			break;
		case PJSIP_EVSUB_STATE_TERMINATED:
			PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_state PJSIP_EVSUB_STATE_TERMINATED dst %s", pres->_Dst.ptr));

			if (event->body.tsx_state.tsx->status_code == PJSIP_SC_REQUEST_TIMEOUT)
			{
				PJ_LOG(5,(THIS_FILE, "PRESENCE: No hay respuesta a la subscripcion al evento de presencia %s", pres->_Dst.ptr));
				//No ha habido respuesta al Request de la subscripcion
				if (pres->nSubsTries < pres->MAX_nSubsTries) 
				{
					//Si no se llega al maximo numero de intentos. Se reintenta con un retardo por si 
					//nos encontramos en una transicion entre servidores principal-reserva
					pres->End();
					pres->Init();

					pres->nSubsTries++;
					terminar = PJ_FALSE;
				}
				else
				{
					terminar = PJ_TRUE;
					PJ_LOG(3,(THIS_FILE, "ERROR: No hay respuesta a la subscripcion al evento de presencia %s", pres->_Dst.ptr));
				}
			}
			else if (event->body.tsx_state.tsx->status_code == PJSIP_SC_CALL_TSX_DOES_NOT_EXIST)
			{
				//El servidor de presencia ha respondido con 481. Call transaction does not exist.
				//Puede ser que se ha conmutado de servidor de presencia y por tanto el nuevo ya no reconoce
				//el refresco de la subscripcion
				//Por tanto se crea una nueva

				PJ_LOG(3,(THIS_FILE, "WARNING: Recibido 481 al intentar subscribirse al envento de presencia %s", pres->_Dst.ptr));
				PJ_LOG(3,(THIS_FILE, "WARNING: Posible conmutacion de servidor presencia. Se reinicia la subscripcion al avento de presencia dst %s", pres->_Dst.ptr));

				if (pres->nSubsTries < pres->MAX_nSubsTries) 
				{
					//Si no se llega al maximo numero de intentos. Se reintenta
					pres->End();
					pres->Init();

					pres->nSubsTries += 1;
					terminar = PJ_FALSE;
				}
				else
				{
					terminar = PJ_TRUE;
					PJ_LOG(3,(THIS_FILE, "ERROR: No es posible la subscripcion al evento de presencia (error 481) %s", pres->_Dst.ptr));
				}
			}
			else
			{
				pres->nSubsTries = 0;
			}

			if (terminar && pres->_SubscriptionStatus != TERMINADA)
			{
				//Ha cambiado el estado de la subscripcion
				pres->_SubscriptionStatus = TERMINADA;

				pres->_Module = NULL;
				pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);

				pres->_PresenceStatus = PJ_FALSE;	//Si la subscripción no está activa entonces la presencia se considera indeterminada. Lo ponemos a false
				if (pres->_PresSubs_callback) (pres->_PresSubs_callback)(pres->_Dst.ptr, pres->_SubscriptionStatus, pres->_PresenceStatus, pres->_Data);				
			}
			break;
		case PJSIP_EVSUB_STATE_UNKNOWN:
			PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_state PJSIP_EVSUB_STATE_UNKNOWN dst %s", pres->_Dst.ptr));
			break;
		}

		/*
		if (state == PJSIP_EVSUB_STATE_TERMINATED) 
		{
			pres->_Module = NULL;
			pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
		} 
		*/
		
		
	}
}

void PresSubs::presence_on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, 
													  pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body)
{
	PresSubs * pres = reinterpret_cast<PresSubs*>(pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id));

	if (pres) 
	{	
		pjsip_pres_status pres_info;
		pjsip_pres_get_status(sub, &pres_info);
		char tuple_id[64];
		char timestamp_aux[64];
		pj_bool_t kamailio_present_server = PJ_FALSE;

		PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_rx_notify Dst %s pres_info._is_valid %d rdata->msg_info.clen->len %d", pres->_Dst.ptr, pres_info._is_valid, rdata->msg_info.clen->len));
		pj_str_t uahdrname = pj_str("User-Agent");
		pjsip_user_agent_hdr *ua_hdr = (pjsip_user_agent_hdr*) pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &uahdrname, NULL);		
		if (ua_hdr != NULL)
		{		
			//Buscamos la palabra Kamailio dentro de la cabecera User-Agent
			//Esta version se ha probado y se ha visto que tiene un comportamiento distinto al de la RFC.
			pj_str_t kamailio = pj_str("Kamailio (4.4.7");			
			pj_str_t ua_hdr1;
			ua_hdr1.ptr = ua_hdr->hvalue.ptr;
			ua_hdr1.slen = ua_hdr->hvalue.slen;
			int i;
			for (i = 0; i < ua_hdr->hvalue.slen; i++)
			{
				if (ua_hdr1.slen >= kamailio.slen)
				{
					pj_str_t ua_hdr2;
					ua_hdr2.ptr = ua_hdr1.ptr;
					ua_hdr2.slen = kamailio.slen;
					if (pj_stricmp(&ua_hdr2, &kamailio) == 0)
					{
						kamailio_present_server = PJ_TRUE;
						break;
					}
				}
				else
				{
					break;
				}
				ua_hdr1.ptr++;
				ua_hdr1.slen--;
			}
		}

		if (kamailio_present_server)
		{
			//El servidor de presencia es el Kamailio 4.4.7. Por tanto procesamos el mensaje segun este tipo de servidor
			//Esta version se ha probado y se ha visto que tiene un comportamiento distinto al de la RFC. Y aqui lo tratamos
			//tal y como lo hemos visto que es muy distinto a como lo hemos visto en otros servidores como el opensips el cual
			//se ve que se comporta como la RFC.

			if ((pres_info._is_valid) && (rdata->msg_info.clen->len != 0))
			{
				//Ha llegado un NOTIFY con xml. Vemos el estado recibido en basic_open.
				for (unsigned int i = 0; i < pres_info.info_cnt; i++)
				{
					PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_rx_notify Dst %s open %d", pres->_Dst.ptr, pres_info.info[i].basic_open));
					if (pres_info.info[i].basic_open && pres->_PresenceStatus == PJ_FALSE)
					{
						//Ha cambiado el estado de presencia
						pres->_PresenceStatus = PJ_TRUE;
						if (pres->_SubscriptionStatus == ACTIVADA) 
						{
							//Si la subscripción no está activa entonces no se envían por la callback
							//notificaciones de estado de presencia
							if (pres->_PresSubs_callback) 
								(pres->_PresSubs_callback)(pres->_Dst.ptr, pres->_SubscriptionStatus, pres->_PresenceStatus, pres->_Data);				
						}
					}
					else if (!pres_info.info[i].basic_open && pres->_PresenceStatus == PJ_TRUE)
					{
						//Ha cambiado el estado de presencia
						pres->_PresenceStatus = PJ_FALSE;
						if (pres->_SubscriptionStatus == ACTIVADA) 
						{
							//Si la subscripción no está activa entonces no se envían por la callback
							//notificaciones de estado de presencia
							if (pres->_PresSubs_callback) 
								(pres->_PresSubs_callback)(pres->_Dst.ptr, pres->_SubscriptionStatus, pres->_PresenceStatus, pres->_Data);				
						}
					}
				}
			}	
			else if (rdata->msg_info.clen->len == 0)
			{
				//Ha llegado un NOTIFY sin XML (content-length = 0). Y se ha apreciado que cuando es así es porque el destino ha dejado de enviar
				//el PUBLISH. Y por lo tanto suponemos que no está presente.
				PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_rx_notify Dst %s SIN XML", pres->_Dst.ptr));
				if (pres->_PresenceStatus == PJ_TRUE)
				{
					//Ha cambiado el estado de presencia
					pres->_PresenceStatus = PJ_FALSE;
					if (pres->_SubscriptionStatus == ACTIVADA) 
					{
						//Si la subscripción no está activa entonces no se envían por la callback
						//notificaciones de estado de presencia
						if (pres->_PresSubs_callback) 
							(pres->_PresSubs_callback)(pres->_Dst.ptr, pres->_SubscriptionStatus, pres->_PresenceStatus, pres->_Data);				
					}
				}
			}
		}
		else
		{
			//Es un servidor que no es Kamailio 4.4.7
			if ((pres_info._is_valid) && (rdata->msg_info.clen->len != 0))
			{
				//Ha llegado un NOTIFY con xml. Vemos el estado recibido en basic_open.
				for (unsigned int i = 0; i < pres_info.info_cnt; i++)
				{
					if (pres_info.info[i].id.slen < (sizeof(tuple_id)/sizeof(char)))
					{
						strncpy(tuple_id, pres_info.info[i].id.ptr, pres_info.info[i].id.slen);
						tuple_id[pres_info.info[i].id.slen] = 0;
					}
					else
					{
						tuple_id[0] = 0;
					}
				
					PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_rx_notify Dst %s id %s last id %s last ts %s open %d", pres->_Dst.ptr, tuple_id, pres->last_valid_tuple_id.ptr, pres->last_timestamp.ptr, pres_info.info[i].basic_open));
					if (pres_info.info[i].basic_open)
					{
						//Es un estado open. Comprobamos que es un timestamp posterior
						if (pres_info.info[i].tuple_node != NULL)
						{
							pj_str_t *timestamp = (pj_str_t *) pjpidf_tuple_get_timestamp(pres_info.info[i].tuple_node);
							if (timestamp != NULL)
							{
								if (timestamp->slen < (sizeof(timestamp_aux)/sizeof(char)))
								{
									strncpy(timestamp_aux, timestamp->ptr, timestamp->slen);
									timestamp_aux[timestamp->slen] = 0;
								}
								else
								{
									timestamp_aux[0] = 0;
								}
								PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_rx_notify Dst %s id %s ts %s open %d", 
									pres->_Dst.ptr, tuple_id, timestamp_aux, pres_info.info[i].basic_open));
								pj_bool_t posterior = PJ_FALSE;
								if (pres->last_timestamp.slen > 0)
								{
									if (pj_strcmp(&pres->last_timestamp, timestamp) < 0)
									{
										posterior = PJ_TRUE;
									}
								}
								else
								{
									posterior = PJ_TRUE;
								}

								if (posterior)
								{
									//El nuevo timestamp es posterior. Nos lo guardamos junto con el id
									if (sizeof(pres->last_timestamp_buf) > timestamp->slen && 
										sizeof(pres->last_valid_tuple_id_buf) > pres_info.info[i].id.slen)
									{
										pj_strcpy(&pres->last_timestamp, timestamp);
										pres->last_timestamp.ptr[timestamp->slen] = 0;
										pj_strcpy(&pres->last_valid_tuple_id, &pres_info.info[i].id);
										pres->last_valid_tuple_id.ptr[pres_info.info[i].id.slen] = 0;
									}

									PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_rx_notify Dst %s PRESENTE", pres->_Dst.ptr));

									if (pres->_PresenceStatus == PJ_FALSE)
									{
										//Ha cambiado el estado de presencia. Llamamos a la callback
										pres->_PresenceStatus = PJ_TRUE;

										if (pres->_SubscriptionStatus == ACTIVADA) 
										{
											//Si la subscripción no está activa entonces no se envían por la callback
											//notificaciones de estado de presencia
											if (pres->_PresSubs_callback) 
												(pres->_PresSubs_callback)(pres->_Dst.ptr, pres->_SubscriptionStatus, pres->_PresenceStatus, pres->_Data);				
										}
									}
								}
							}
						}
					}
					else 
					{
						//Hemos recibido un estado closed. 
						//Consideramos el estado como no presente si esta es la primera notificacion recibida
						//o si el id corresponde con el ultimo recibido con estado open
						if (pres->notify_after_initial || 
							pj_strcmp(&pres->last_valid_tuple_id, &pres_info.info[i].id) == 0)
						{
							PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_rx_notify Dst %s NO PRESENTE", pres->_Dst.ptr));

							if (pres->_PresenceStatus == PJ_TRUE)
							{
								//Ha cambiado el estado de presencia. Llamamos a la callback
								pres->_PresenceStatus = PJ_FALSE;

								if (pres->_SubscriptionStatus == ACTIVADA) 
								{
									//Si la subscripción no está activa entonces no se envían por la callback
									//notificaciones de estado de presencia
									if (pres->_PresSubs_callback) 
										(pres->_PresSubs_callback)(pres->_Dst.ptr, pres->_SubscriptionStatus, pres->_PresenceStatus, pres->_Data);				
								}						
							}
						}
					}
				}
			}	
			else if (rdata->msg_info.clen->len == 0)
			{
				//Ha llegado un NOTIFY sin XML (content-length = 0). 
				//Segun la RFC3856 apartado 6.6.2, un notify despues del un subscribe inicial o refresco 
				//sin body se puede considerar como estado no definido.
				//En este caso lo consideraremos no presente.
				PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_rx_notify Dst %s SIN XML notify_after_initial %d notify_after_refresh %d", pres->_Dst.ptr, pres->notify_after_initial, pres->notify_after_refresh));
				if (pres->notify_after_initial || pres->notify_after_refresh)
				{
					PJ_LOG(5,(THIS_FILE, "PRESENCE: PresSubs::presence_on_rx_notify Dst %s NO PRESENTE", pres->_Dst.ptr));

					if (pres->_PresenceStatus == PJ_TRUE)
					{
						//Ha cambiado el estado de presencia. Llamamos a la callback
						pres->_PresenceStatus = PJ_FALSE;

						if (pres->_SubscriptionStatus == ACTIVADA) 
						{
							//Si la subscripción no está activa entonces no se envían por la callback
							//notificaciones de estado de presencia
							if (pres->_PresSubs_callback) 
								(pres->_PresSubs_callback)(pres->_Dst.ptr, pres->_SubscriptionStatus, pres->_PresenceStatus, pres->_Data);				
						}
					}
				}
			}
		}

		pres->notify_after_initial = PJ_FALSE;
		pres->notify_after_refresh = PJ_FALSE;
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
 * presence_on_client_refresh.	...
 * Funcion callback que se llama cada vez que hay que refrescar la subscripcion.
 * Se utiliza para poder agregar la cabecera WG67-version que corresponde
 */
void PresSubs::presence_on_client_refresh(pjsip_evsub *sub)
{
	pj_status_t status;
	pjsip_tx_data *tdata;

	status = pjsip_pres_initiate(sub, -1, &tdata);
	if (status == PJ_SUCCESS) {

		//Establecemos aqui la cabecera WG67-version con la version de telefonia. Porque se supone
		//que solo nos subscribimos en el servidor de presencia para telefonia
		//En la funcion OnTxRequest de SipCall.cpp es donde se establece esta cabecera para todos
		//los paquetes SIP, siempre y cuando no se hayan establecido antes, como es este el caso.
		//Se fuerza aqui la cabecera porque el agente que subscribe al servidor de presencia es el
		//Nodebox, el cual esta definido como un agente radio. 
		SipCall::Wg67VersionSet(tdata, &SipCall::gWG67VersionTelefValue);

		PresSubs * pres = reinterpret_cast<PresSubs*>(pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id));
		pres->notify_after_refresh = PJ_TRUE;

	    pjsip_pres_send_request(sub, tdata);
    }
}

pjsip_evsub_user PresSubs::presence_callback = 
{
	&PresSubs::presence_on_state,  
	NULL,
	NULL,
	&PresSubs::presence_on_rx_notify,
	&PresSubs::presence_on_client_refresh,
	NULL
};

/**
 * PresSubs.	...
 * Constructor
 * @param	dst					Uri del destino al que nos queremos subscribir
 * @param	PresSubs_callback	Callback para notificar cambios de estado. 
 * @param	data				Datos arbitrarios (normalmente un puntero al objeto que llama a este constructor)
 * @return	-1 si hay error.
 */
PresSubs::PresSubs(char *dst, void (*PresSubs_callback)(char *dst, int subscription_status, int presence_status, void *data), void *data)
{
	_Data = data;
	_Pool = NULL;
	_Module = NULL;
	_Pool = pjsua_pool_create(NULL, 512, 256);
	_PresSubs_callback = PresSubs_callback;
	pj_strdup2_with_null(_Pool, &_Dst, dst);	
	_SubscriptionStatus = REPOSO;
	_PresenceStatus = PJ_FALSE;
	notify_after_initial = PJ_TRUE;
	notify_after_refresh = PJ_FALSE;
	nSubsTries = 0;
	if (pjsip_cfg()->tsx.tsx_tout > 0)
	{
		MAX_nSubsTries = PresSubs::MAX_SUBS_TIME / pjsip_cfg()->tsx.tsx_tout;
	}
	else
	{
		MAX_nSubsTries = 12;
	}

	last_valid_tuple_id.ptr = last_valid_tuple_id_buf;
	last_valid_tuple_id_buf[0] = 0;
	last_valid_tuple_id.slen = 0;
	last_timestamp.ptr = last_timestamp_buf;
	last_timestamp_buf[0] = 0;
	last_timestamp.slen = 0;
}

PresSubs::~PresSubs()
{
	if (_Pool)
	{
		pj_pool_release(_Pool);
	}
}

/**
 * Init.	...
 * Inicia la subscripción al evento de presencia. Después de crear el objeto habría que llamar a esta funcion
 * @return	-1 si hay error.
 */
int PresSubs::Init()
{	
	pj_status_t st;
	pjsip_dialog * Dlg;
	notify_after_initial = PJ_TRUE;
	notify_after_refresh = PJ_FALSE;

	pjsua_acc_id acc_id = pjsua_acc_get_default();
	if (!pjsua_acc_is_valid(acc_id))
	{
		PJ_LOG(3,(__FILE__, "ERROR: Account %d del agente no valida, al intentar crear subscripcion al evento de presencia", acc_id, _Dst));
		return -1;
	}

	pjsua_acc * acc = &pjsua_var.acc[acc_id];

	pj_str_t contact;
	if (acc->contact.slen) 
	{
		contact = acc->contact;
	} 
	else 
	{
		st = pjsua_acc_create_uac_contact(_Pool, &contact, acc_id, &_Dst, NULL);
		if (st != PJ_SUCCESS)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Unable to generate Contact header for PRESENCE-IN %s", _Dst)); 
			return -1;
		}
	}

	st = pjsip_dlg_create_uac(pjsip_ua_instance(), &acc->cfg.id, &contact, &_Dst, NULL, &Dlg);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Unable to create dialog for PRESENCE-IN %s", _Dst)); 
		return -1;
	}

	pjsip_dlg_inc_lock(Dlg);

	st = pjsip_pres_create_uac(Dlg, &presence_callback, PJSIP_EVSUB_NO_EVENT_ID, &_Module);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Unable to create PRESENCE-IN client %s", _Dst)); 
		_Module = NULL;
		if (Dlg) pjsip_dlg_dec_lock(Dlg);
		return -1;
	}

	if (acc->cfg.transport_id != PJSUA_INVALID_ID) 
	{
		pjsip_tpselector tp_sel;

		pjsua_init_tpselector(acc->cfg.transport_id, &tp_sel);
		pjsip_dlg_set_transport(Dlg, &tp_sel);
	}

	if (!pj_list_empty(&acc->route_set)) 
	{
		pjsip_dlg_set_route_set(Dlg, &acc->route_set);
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

		route = (pjsip_route_hdr *) pjsip_parse_hdr( Dlg->pool, &hname, 
						uri, strlen(uri),
						NULL);
		if (route != NULL)
			pj_list_push_back(&route_set, route);

		pjsip_dlg_set_route_set(Dlg, &route_set);

		/*
		* Note that Route URI SHOULD have an ";lr" parameter!
		*/
	}
#endif

	if (acc->cred_cnt) 
	{
		pjsip_auth_clt_set_credentials(&Dlg->auth_sess, acc->cred_cnt, acc->cred);
	}
	pjsip_auth_clt_set_prefs(&Dlg->auth_sess, &acc->cfg.auth_pref);

	pjsip_evsub_set_mod_data(_Module, pjsua_var.mod.id, this);

	pjsip_tx_data *tdata;
	st = pjsip_pres_initiate(_Module, 60, &tdata);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "Unable to create initial presence event SUBSCRIBE %s", _Dst)); 
		if (_Module) pjsip_pres_terminate(_Module, PJ_FALSE);
		_Module = NULL;
		if (Dlg) pjsip_dlg_dec_lock(Dlg);
		return -1;
	}

	//Establecemos aqui la cabecera WG67-version con la version de telefonia. Porque se supone
	//que solo nos subscribimos en el servidor de presencia para telefonia
	//En la funcion OnTxRequest de SipCall.cpp es donde se establece esta cabecera para todos
	//los paquetes SIP, siempre y cuando no se hayan establecido antes, como es este el caso.
	//Se fuerza aqui la cabecera porque el agente que subscribe al servidor de presencia es el
	//Nodebox, el cual esta definido como un agente radio. 
	SipCall::Wg67VersionSet(tdata, &SipCall::gWG67VersionTelefValue);

	pjsua_process_msg_data(tdata, NULL);

	st = pjsip_pres_send_request(_Module, tdata);
	if (st != PJ_SUCCESS)
	{
		PJ_LOG(3,(__FILE__, "Unable to send initial presence event SUBSCRIBE %s", _Dst)); 
		if (_Module) pjsip_pres_terminate(_Module, PJ_FALSE);
		_Module = NULL;
		if (Dlg) pjsip_dlg_dec_lock(Dlg);
		return -1;
	}

	if (Dlg) pjsip_dlg_dec_lock(Dlg);		

	return 0;
}

/**
 * End.	...
 * Finaliza la subscripción al evento de presencia. Envía el mensaje de SUBSCRIPTION con expire a cero para 
 * finalizar la subscripción. Hay que llamar a esta función antes de destruir el objeto con delete.
 * @return	-1 si hay error.
 */
int PresSubs::End()
{
	pjsip_evsub_state status = PJSIP_EVSUB_STATE_NULL;

	if (_Module != NULL) 
	{
		if (pjsip_evsub_get_state(_Module) != PJSIP_EVSUB_STATE_TERMINATED) 
		{
			pjsip_evsub_set_mod_data(_Module, pjsua_var.mod.id, NULL);

			pjsip_tx_data *tdata;
			pj_status_t st = pjsip_pres_initiate(_Module, 0, &tdata);
			if (st == PJ_SUCCESS) 
			{
				pjsua_process_msg_data(tdata, NULL);
				SipCall::Wg67VersionSet(tdata, &SipCall::gWG67VersionTelefValue);
				st = pjsip_pres_send_request(_Module, tdata);
				if (st == PJ_SUCCESS)
				{
					//Esperamos a que se haya recibido el 200 ok
					int tries = 10;
					status = pjsip_evsub_get_state(_Module);
					while (status != PJSIP_EVSUB_STATE_TERMINATED && _Module)
					{
						pj_thread_sleep(20);
						tries--;
						if (tries == 0) break;
						if (_Module)
						{
							status = pjsip_evsub_get_state(_Module);
						}
					}
				}
				else
				{
					if (_Module) 
					{
						pjsip_pres_terminate(_Module, PJ_FALSE);
						_Module = NULL;
					}
				}
			}
			else
			{
				if (_Module) 
				{
					pjsip_pres_terminate(_Module, PJ_FALSE);
					_Module = NULL;
				}
			}
		}
		else
		{
			_Module = NULL;
		}
	}

	return 0;
}



