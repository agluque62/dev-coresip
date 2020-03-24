#include <pjsip-simple/errno.h>
#include <pjsip-simple/evsub_msg.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_dialog.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include "dlgsub.h"


#define THIS_FILE						"dlgsub.c"
#define DIALOG_DEFAULT_EXPIRES		3600

/*
* Dialog module (mod-dialog)
*/
static struct pjsip_module mod_dialog = 
{
	NULL, NULL,			    /* prev, next.			*/
	{ "mod-dialog", 14 },	    /* Name.				*/
	-1,				    /* Id				*/
	PJSIP_MOD_PRIORITY_DIALOG_USAGE,/* Priority				*/
	NULL,			    /* load()				*/
	NULL,			    /* start()				*/
	NULL,			    /* stop()				*/
	NULL,			    /* unload()				*/
	NULL,			    /* on_rx_request()			*/
	NULL,			    /* on_rx_response()			*/
	NULL,			    /* on_tx_request.			*/
	NULL,			    /* on_tx_response()			*/
	NULL,			    /* on_tsx_state()			*/
};


/*
* Dialog message body type.
*/
typedef enum content_type_e
{
	CONTENT_TYPE_NONE,
	CONTENT_TYPE_DIDF,
} content_type_e;

/*
* This structure describe a subscription, for both subscriber and notifier.
*/
struct pjsip_dialogsub
{
	pjsip_evsub		*sub;		/**< Event subscribtion record.	    */
	pjsip_dialog	*dlg;		/**< The dialog.		    */
	content_type_e	 content_type;	/**< Content-Type.		    */
	pjsip_evsub_user	 user_cb;	/**< The user callback.		    */
	pj_uint32_t 	notify_version; /*Version del body del notify*/
};


typedef struct pjsip_dialogsub pjsip_dialogsub;


/*
* Forward decl for evsub callback.
*/
static void dialog_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);
static void dialog_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx, pjsip_event *event);
static void dialog_on_evsub_rx_refresh( pjsip_evsub *sub, 
												 pjsip_rx_data *rdata,
												 int *p_st_code,
												 pj_str_t **p_st_text,
												 pjsip_hdr *res_hdr,
												 pjsip_msg_body **p_body);
static void dialog_on_evsub_rx_notify( pjsip_evsub *sub, 
												pjsip_rx_data *rdata,
												int *p_st_code,
												pj_str_t **p_st_text,
												pjsip_hdr *res_hdr,
												pjsip_msg_body **p_body);
static void dialog_on_evsub_client_refresh(pjsip_evsub *sub);
static void dialog_on_evsub_server_timeout(pjsip_evsub *sub);

static int xml_print_body(struct pjsip_msg_body *msg_body, char *buf, pj_size_t size);
static void* xml_clone_data(pj_pool_t *pool, const void *data, unsigned len);

/*
* Event subscription callback for dialog.
*/
static pjsip_evsub_user dialog_user = 
{
	&dialog_on_evsub_state,
	&dialog_on_evsub_tsx_state,
	&dialog_on_evsub_rx_refresh,
	&dialog_on_evsub_rx_notify,
	&dialog_on_evsub_client_refresh,
	&dialog_on_evsub_server_timeout,
};


/*
* Some static constants.
*/
static const pj_str_t STR_EVENT				= { "Event", 5 };
static const pj_str_t STR_DIALOG			= { "dialog", 6 };
static const pj_str_t STR_APPLICATION		= { "application", 11 };
static const pj_str_t STR_DIDF_XML			= { "dialog-info+xml", 15};
static const pj_str_t STR_APP_CIDF_XML		= { "application/dialog-info+xml", 27 };

static pj_str_t FULL = { "full", 4 };			//state
static pj_str_t RECIPIENT = { "recipient", 9 };//direction
static pj_str_t INITIATOR = { "initiator", 9 };
static pj_str_t LOCAL = { "local", 5 };//direction
static pj_str_t REMOTE = { "remote", 6 };//direction
static pj_str_t DIALOG_INFO = { "dialog-info", 11 };
static pj_str_t XMLNS = { "xmlns", 5 };
static pj_str_t DIDF_XMLNS = { "urn:ietf:params:xml:ns:dialog-info", 34 };
static pj_str_t DIALOG = { "dialog", 6 };
static pj_str_t ENTITY = { "entity", 6};
static pj_str_t STATE = { "state", 5};
static pj_str_t VERSION = { "version", 7};
static pj_str_t DIALOG_ID = { "id", 2 };
static pj_str_t CALL_ID = { "call-id", 7 };
static pj_str_t LOCAL_TAG = { "local-tag", 9 };
static pj_str_t REMOTE_TAG = { "remote-tag", 10 };
static pj_str_t DIRECTION = { "direction", 9 };
static pj_str_t DURATION = { "duration", 8 };
static pj_str_t IDENTITY = { "identity", 8 };
static pj_str_t DISPLAY = { "display", 7 };
static pj_str_t TARGET = { "target", 6 };
static pj_str_t URI = { "uri", 3 };
static pj_str_t ISFOCUS = { "isfocus", 7 };
static pj_str_t TRUE = { "true", 4 };
static pj_str_t FALSE = { "false", 5 };

static const char *FSM_state_names[] =
{
    "null",
    "trying",
    "trying",
    "early",
    "early",
    "confirmed",
    "terminated",
    "terminated",
};

/*
* Init dialog module.
*/
PJ_DEF(pj_status_t) pjsip_dialog_init_module( pjsip_endpoint *endpt)
{
	pj_status_t status;
	pj_str_t accept[1];

	/* Check arguments. */
	PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

	/* Must have not been registered */
	PJ_ASSERT_RETURN(mod_dialog.id == -1, PJ_EINVALIDOP);

	/* Register to endpoint */
	status = pjsip_endpt_register_module(endpt, &mod_dialog);
	if (status != PJ_SUCCESS)
		return status;

	accept[0] = STR_APP_CIDF_XML;

	/* Register event package to event module. */
	status = pjsip_evsub_register_pkg( &mod_dialog, &STR_DIALOG, 
		DIALOG_DEFAULT_EXPIRES, 
		PJ_ARRAY_SIZE(accept), accept);
	if (status != PJ_SUCCESS) {
		pjsip_endpt_unregister_module(endpt, &mod_dialog);
		return status;
	}

	return PJ_SUCCESS;
}


/*
* Get dialog module instance.
*/
PJ_DEF(pjsip_module*) pjsip_dialog_instance(void)
{
	return &mod_dialog;
}


/*
* Create client subscription.
*/
PJ_DEF(pj_status_t) pjsip_dialog_create_uac( pjsip_dialog *dlg,
														const pjsip_evsub_user *user_cb,
														unsigned options,
														pjsip_evsub **p_evsub )
{
	pj_status_t status;
	pjsip_dialogsub *dlgsubs;
	pjsip_evsub *sub;

	PJ_ASSERT_RETURN(dlg && p_evsub, PJ_EINVAL);

	pjsip_dlg_inc_lock(dlg);

	/* Create event subscription */
	status = pjsip_evsub_create_uac( dlg, &dialog_user, &STR_DIALOG, options, &sub);
	if (status != PJ_SUCCESS)
		goto on_return;

	/* Create dialog */
	dlgsubs = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_dialogsub);
	dlgsubs->dlg = dlg;
	dlgsubs->sub = sub;
	if (user_cb)
		pj_memcpy(&dlgsubs->user_cb, user_cb, sizeof(pjsip_evsub_user));

	/* Attach to evsub */
	pjsip_evsub_set_mod_data(sub, mod_dialog.id, dlgsubs);

	*p_evsub = sub;

on_return:
	pjsip_dlg_dec_lock(dlg);
	return status;
}


/*
* Create server subscription.
*/
PJ_DEF(pj_status_t) pjsip_dialog_create_uas( pjsip_dialog *dlg,
														const pjsip_evsub_user *user_cb,
														pjsip_rx_data *rdata,
														pjsip_evsub **p_evsub )
{
	pjsip_accept_hdr *accept;
	pjsip_event_hdr *event;
	content_type_e content_type = CONTENT_TYPE_NONE;
	pjsip_evsub *sub;
	pjsip_dialogsub *dlgsubs;
	pj_status_t status;

	/* Check arguments */
	PJ_ASSERT_RETURN(dlg && rdata && p_evsub, PJ_EINVAL);

	/* Must be request message */
	PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG, PJSIP_ENOTREQUESTMSG);

	/* Check that request is SUBSCRIBE */
	PJ_ASSERT_RETURN(pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
		&pjsip_subscribe_method)==0,
		PJSIP_SIMPLE_ENOTSUBSCRIBE);

	/* Check that Event header contains "dialog" */
	event = (pjsip_event_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_EVENT, NULL);
	if (!event) {
		return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
	}
	if (pj_stricmp(&event->event_type, &STR_DIALOG) != 0) {
		return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_EVENT);
	}

	/* Check that request contains compatible Accept header. */
	accept = (pjsip_accept_hdr*)pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_ACCEPT, NULL);
	if (accept) {
		unsigned i;
		for (i=0; i<accept->count; ++i) {
			if (pj_stricmp(&accept->values[i], &STR_APP_CIDF_XML)==0) {
				content_type = CONTENT_TYPE_DIDF;
				break;
			} 
		}

		if (i==accept->count) {
			/* Nothing is acceptable */
			return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE);
		}

	} else {
		/* No Accept header.
		* Treat as "application/dialog-info+xml"
		*/
		content_type = CONTENT_TYPE_DIDF;
	}

	/* Lock dialog */
	pjsip_dlg_inc_lock(dlg);

	/* Create server subscription */
	status = pjsip_evsub_create_uas( dlg, &dialog_user, rdata, 0, &sub);
	if (status != PJ_SUCCESS)
		goto on_return;

	/* Create server dialog subscription */
	dlgsubs = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_dialogsub);
	dlgsubs->dlg = dlg;
	dlgsubs->sub = sub;
	dlgsubs->content_type = content_type;
	if (user_cb)
		pj_memcpy(&dlgsubs->user_cb, user_cb, sizeof(pjsip_evsub_user));

	dlgsubs->notify_version = 0;

	/* Attach to evsub */
	pjsip_evsub_set_mod_data(sub, mod_dialog.id, dlgsubs);

	/* Done: */
	*p_evsub = sub;

on_return:
	pjsip_dlg_dec_lock(dlg);
	return status;
}


/*
* Forcefully terminate dialog subscription.
*/
PJ_DEF(pj_status_t) pjsip_dialog_terminate( pjsip_evsub *sub, pj_bool_t notify )
{
	return pjsip_evsub_terminate(sub, notify);
}

/*
* Create SUBSCRIBE
*/
PJ_DEF(pj_status_t) pjsip_dialog_initiate( pjsip_evsub *sub, pj_int32_t expires, pjsip_tx_data **p_tdata)
{
	return pjsip_evsub_initiate(sub, &pjsip_subscribe_method, expires, p_tdata);
}

/*
* Accept incoming subscription.
*/
PJ_DEF(pj_status_t) pjsip_dialog_accept( pjsip_evsub *sub, pjsip_rx_data *rdata, int st_code, const pjsip_hdr *hdr_list )
{
	return pjsip_evsub_accept( sub, rdata, st_code, hdr_list );
}


/*
* Create message body.
*/
static pj_status_t dialog_create_msg_body( pjsua_call_id callid, pjsip_dialogsub *dlgsubs, pjsip_tx_data *tdata)
{
	/* Parametros que necesitamos para crear el body:
	version: Se obtiene de dlgsubs->status.version. Cada vez que enviamos uno lo incrementamos.
	state:	Lo pongo siempre valor full. Solo enviamos documentos completos
	entity: Es la uri del que manda el notify. No es el contact. Es la uri del from-local de la subscripcion

	De cada una de las llamadas entrates al que envia el notify que estan en estado early 
	(de momento, solo este estado ya que es para pick-up)
	se envía la siguiente info en cada dialogo:

	id: De momento coincide con el call-id
	call-id: el call id de la llamada
	local-tag:
	remote-tag:
	direction: Que en nuestro caso tiene que ser "recipient"
	duration:	El tiempo que dura la llamada. Creo que se puede obtener.
	state: En nuestro caso sera early

	Para el nodo local necesitamos:
	identity: el display name y la uri del que envia el notify. No el contact.
	target: es el contact, si tiene parametros como isfocus hay que incluirlos.

	Para el nodo remote necesitamos:
	identity: el display name y la uri del llamante. No el contact.
	target: es el contact, si tiene parametros como isfocus hay que incluirlos.

	*/

	pj_status_t st = PJ_SUCCESS;
	unsigned int i = 0;
	pjsua_call_info info;	
	pjsua_call *call;
	pjsip_dialog *dlg;
	pj_str_t *local_tag;
	pj_str_t *remote_tag;
	pj_time_val duration_val;	
	pj_str_t local_identity_display;
	pjsip_uri *local_identity_uri;
	char local_identity_uri_char[PJSIP_MAX_URL_SIZE];
	pj_str_t local_identity_uri_str;
	pjsip_uri *local_target_uri;
	char local_target_uri_char[PJSIP_MAX_URL_SIZE];
	pj_str_t local_target_uri_str;
	pj_str_t remote_identity_display;
	pjsip_uri *remote_identity_uri;
	char remote_identity_uri_char[PJSIP_MAX_URL_SIZE];
	pj_str_t remote_identity_uri_str;
	pjsip_uri *remote_target_uri;
	char remote_target_uri_char[PJSIP_MAX_URL_SIZE];
	pj_str_t remote_target_uri_str;
	pj_str_t state_text;
	pjsip_param *otherparam;
	pjsip_param *p;
	pjdidf_dialog_info *dialog_info_node;
	pj_xml_node *dialog_node;
	pj_xml_node *local_node;
	pj_xml_node *identity_node;
	pj_xml_node *target_node;
	pj_xml_node *remote_node;
	pjsip_msg_body *body;

	dlgsubs->notify_version++;	//version
		
	//entity
	dlg = dlgsubs->dlg;

	if (dlg->local.info != NULL)
	{
		local_identity_uri = (pjsip_uri *) pjsip_uri_get_uri(dlg->local.info->uri);
		pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, local_identity_uri, local_identity_uri_char, PJSIP_MAX_URL_SIZE);
	}
	else
	{
		local_identity_uri_char[0] = '\0';
	}
	local_identity_uri_str = pj_str(local_identity_uri_char);

	//Creamos el nodo dialog info
	dialog_info_node = pjdidf_create(tdata->pool, dlgsubs->notify_version, &FULL, &local_identity_uri_str);	 
	PJ_ASSERT_RETURN(dialog_info_node != NULL, PJ_ENOMEM);

	st = acquire_call("dialog_create_msg_body()", callid, &call, &dlg);
	if (st != PJ_SUCCESS) return PJ_ENOTFOUND;

	st = pjsua_call_get_info(callid, &info);
	if (st != PJ_SUCCESS)
	{
		pjsip_dlg_dec_lock(dlg);
		return PJ_ENOTFOUND;
	}

	local_tag = &dlg->local.info->tag;
	remote_tag = &dlg->remote.info->tag;

	pj_gettimeofday(&duration_val);
	PJ_TIME_VAL_SUB(duration_val, call->start_time);				
					
	if (dlg->local.info != NULL)
	{
		local_identity_display = pjsip_uri_get_display(dlg->local.info->uri);
		local_identity_uri = (pjsip_uri *) pjsip_uri_get_uri(dlg->local.info->uri);
		if (local_identity_uri != NULL)
			pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, local_identity_uri, local_identity_uri_char, PJSIP_MAX_URL_SIZE);
		else
			local_identity_uri_char[0] = '\0';
	}
	else
	{
		local_identity_uri_char[0] = '\0';
	}
	local_identity_uri_str = pj_str(local_identity_uri_char);

	if (dlg->local.contact != NULL)
	{
		local_target_uri = (pjsip_uri *) pjsip_uri_get_uri(dlg->local.contact->uri);
		if (local_target_uri != NULL)
			pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, local_target_uri, local_target_uri_char, PJSIP_MAX_URL_SIZE);
		else
			local_target_uri_char[0] = '\0';
	}
	else
	{
		local_target_uri_char[0] = '\0';
	}
	local_target_uri_str = pj_str(local_target_uri_char);

	if (dlg->remote.info != NULL)
	{
		remote_identity_display = pjsip_uri_get_display(dlg->remote.info->uri);
		remote_identity_uri = (pjsip_uri *) pjsip_uri_get_uri(dlg->remote.info->uri);
		if (remote_identity_uri != NULL)
			pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, remote_identity_uri, remote_identity_uri_char, PJSIP_MAX_URL_SIZE);
		else
			remote_identity_uri_char[0] = '\0';
	}
	else
	{
		remote_identity_uri_char[0] = '\0';
	}
	remote_identity_uri_str = pj_str(remote_identity_uri_char);

	if (dlg->remote.contact != NULL)
	{
		remote_target_uri = (pjsip_uri *) pjsip_uri_get_uri(dlg->remote.contact->uri);
		if (remote_target_uri != NULL)
			pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, remote_target_uri, remote_target_uri_char, PJSIP_MAX_URL_SIZE);
		else
			remote_target_uri_char[0] = '\0';
	}
	else
	{
		remote_target_uri_char[0] = '\0';
	}
	remote_target_uri_str = pj_str(remote_target_uri_char);

	if (info.role == PJSIP_ROLE_UAS)
	{
		dialog_node = pjdidf_dialog_add_dialog(tdata->pool, dialog_info_node, &info.call_id, &info.call_id, 
									local_tag, remote_tag, &RECIPIENT);
	}
	else
	{
		dialog_node = pjdidf_dialog_add_dialog(tdata->pool, dialog_info_node, &info.call_id, &info.call_id, 
									local_tag, remote_tag, &INITIATOR);
	}
	state_text = pj_str((char *) FSM_state_names[info.state]);
	pjdidf_dialog_add_state(tdata->pool, dialog_node, &state_text);
	pjdidf_dialog_add_duration(tdata->pool, dialog_node, duration_val.sec);

	local_node = pjdidf_dialog_add_local_remote(tdata->pool, dialog_node, &LOCAL);
	if (local_identity_uri_str.slen > 0)
		identity_node = pjdidf_dialog_add_identity(tdata->pool, local_node, &local_identity_display, &local_identity_uri_str);
	if (local_target_uri_str.slen > 0)
	{
		target_node = pjdidf_dialog_add_target(tdata->pool, local_node, &local_target_uri_str);
		if (dlg->local.contact != NULL)
		{
			if (dlg->local.contact->focus)
			{
				pjdidf_dialog_add_param(tdata->pool, target_node, &ISFOCUS, &TRUE);
			}

			otherparam = &dlg->local.contact->other_param;
			p = otherparam->next;
			while (p != otherparam)
			{
				if (p->name.ptr != NULL && p->name.slen > 0)
				{
					if ((p->value.ptr == NULL) || (p->value.ptr != NULL && p->value.slen == 0))
					{
						pjdidf_dialog_add_param(tdata->pool, target_node, &p->name, &TRUE);
					}
					else
					{
						pjdidf_dialog_add_param(tdata->pool, target_node, &p->name, &p->value);
					}
				}					
				p = otherparam->next;
			}
		}
	}

	remote_node = pjdidf_dialog_add_local_remote(tdata->pool, dialog_node, &REMOTE);
	if (remote_identity_uri_str.slen > 0)
		identity_node = pjdidf_dialog_add_identity(tdata->pool, remote_node, &remote_identity_display, &remote_identity_uri_str);
	if (remote_target_uri_str.slen > 0)
	{
		target_node = pjdidf_dialog_add_target(tdata->pool, remote_node, &remote_target_uri_str);

		if (dlg->remote.contact != NULL)
		{
			if (dlg->remote.contact->focus)
			{
				pjdidf_dialog_add_param(tdata->pool, target_node, &ISFOCUS, &TRUE);
			}
			otherparam = &dlg->remote.contact->other_param;
			p = otherparam->next;
			while (p != otherparam)
			{
				if (p->name.ptr != NULL && p->name.slen > 0)
				{
					if ((p->value.ptr == NULL) || (p->value.ptr != NULL && p->value.slen == 0))
					{
						pjdidf_dialog_add_param(tdata->pool, target_node, &p->name, &TRUE);
					}
					else
					{
						pjdidf_dialog_add_param(tdata->pool, target_node, &p->name, &p->value);
					}
				}					
				p = otherparam->next;
			}
		}
	}
				
	pjsip_dlg_dec_lock(dlg);

	body = PJ_POOL_ZALLOC_T(tdata->pool, pjsip_msg_body);
	body->data = dialog_info_node;
	body->content_type.type = STR_APPLICATION;
	body->content_type.subtype = STR_DIDF_XML;
	body->print_body = &xml_print_body;
	body->clone_data = &xml_clone_data;

	tdata->msg->body = body;

	return PJ_SUCCESS;
}

/*
* Create NOTIFY
* @param callid. Call id de la llamada a la que wse refiere el dialogo sobre el que se envia la notificacion
* @sub.	objeto de la subscripcion al evento de dialogo
....
*/
PJ_DEF(pj_status_t) pjsip_dialog_notify( pjsua_call_id callid, pjsip_evsub *sub,
												  pjsip_evsub_state state,
												  const pj_str_t *state_str,
												  const pj_str_t *reason,
												  pj_bool_t with_body)
{
	pjsip_dialogsub *dlgsubs;
	pjsip_tx_data *tdata;
	pj_status_t status = PJ_SUCCESS;

	/* Check arguments. */
	PJ_ASSERT_RETURN(sub, PJ_EINVAL);

	/* Get the dialog object. */
	dlgsubs = (pjsip_dialogsub*) pjsip_evsub_get_mod_data(sub, mod_dialog.id);
	PJ_ASSERT_RETURN(dlgsubs != NULL, PJSIP_SIMPLE_ENODIALOG);

	/* Must have dialog info valid, unless state is 
	* PJSIP_EVSUB_STATE_TERMINATED. This could happen if subscription
	* has not been active (e.g. we're waiting for user authorization)
	* and remote cancels the subscription.
	*/
	PJ_ASSERT_RETURN(state==PJSIP_EVSUB_STATE_TERMINATED ||
		dlgsubs->notify_version > 0, PJSIP_SIMPLE_ENODIALOGINFO);

	/* Lock object. */
	pjsip_dlg_inc_lock(dlgsubs->dlg);

	/* Create the NOTIFY request. */
	status = pjsip_evsub_notify( sub, state, state_str, reason, &tdata);
	if (status != PJ_SUCCESS)
		goto on_return;


	if (with_body && callid != PJSUA_INVALID_ID)
	{
		/* Create message body to reflect the dialog status. 
		* Only do this if we have dialog info to send (see above).
		*/
		status = dialog_create_msg_body( callid, dlgsubs, tdata );
		if (status != PJ_SUCCESS)
			goto on_return;
	}


on_return:
	pjsip_dlg_dec_lock(dlgsubs->dlg);

	if (status == PJ_SUCCESS)
	{
		pjsip_dialog_send_request(sub, tdata);
	}

	return status;
}

/*
* Create NOTIFY that reflect current state and send.
* @param callid. Call id de la llamada a la que wse refiere el dialogo sobre el que se envia la notificacion
* @sub.	objeto de la subscripcion al evento de dialogo
* @param with_body. Includes body in the notify
*/
PJ_DEF(pj_status_t) pjsip_dialog_current_notify( pjsua_call_id callid, pjsip_evsub *sub, pj_bool_t with_body)
{
	pjsip_dialogsub *dlgsubs;
	pjsip_tx_data *tdata;
	pj_status_t status = PJ_SUCCESS;

	/* Check arguments. */
	PJ_ASSERT_RETURN(sub, PJ_EINVAL);

	/* Get the presence object. */
	dlgsubs = (pjsip_dialogsub*) pjsip_evsub_get_mod_data(sub, mod_dialog.id);
	PJ_ASSERT_RETURN(dlgsubs != NULL, PJSIP_SIMPLE_ENODIALOG);

	/* We may not have a dialog info yet, e.g. when we receive SUBSCRIBE
	* to refresh subscription while we're waiting for user authorization.
	*/
	//PJ_ASSERT_RETURN(dlgsubs->status.version > 0, 
	//		       PJSIP_SIMPLE_ENODIALOGINFO);


	/* Lock object. */
	pjsip_dlg_inc_lock(dlgsubs->dlg);

	/* Create the NOTIFY request. */
	status = pjsip_evsub_current_notify( sub, &tdata);
	if (status != PJ_SUCCESS)
		goto on_return;

	if (with_body && callid != PJSUA_INVALID_ID)
	{
		/* Create message body to reflect the presence status. */
		status = dialog_create_msg_body( callid, dlgsubs, tdata );
		if (status != PJ_SUCCESS)
			goto on_return;
	}

on_return:
	pjsip_dlg_dec_lock(dlgsubs->dlg);

	if (status == PJ_SUCCESS)
	{
		pjsip_dialog_send_request(sub, tdata);
	}

	return status;
}

/*
* Send request.
*/
PJ_DEF(pj_status_t) pjsip_dialog_send_request( pjsip_evsub *sub, pjsip_tx_data *tdata )
{
	return pjsip_evsub_send_request(sub, tdata);
}

/*
* This callback is called by event subscription when subscription
* state has changed.
*/
static void dialog_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
	pjsip_dialogsub *dlgsubs;

	dlgsubs = (pjsip_dialogsub*) pjsip_evsub_get_mod_data(sub, mod_dialog.id);
	PJ_ASSERT_ON_FAIL(dlgsubs!=NULL, {return;});

	if (dlgsubs->user_cb.on_evsub_state)
		(*dlgsubs->user_cb.on_evsub_state)(sub, event);
}

/*
* Called when transaction state has changed.
*/
static void dialog_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx, pjsip_event *event)
{
	pjsip_dialogsub *dlgsubs;

	dlgsubs = (pjsip_dialogsub*) pjsip_evsub_get_mod_data(sub, mod_dialog.id);
	PJ_ASSERT_ON_FAIL(dlgsubs!=NULL, {return;});

	if (dlgsubs->user_cb.on_tsx_state)
		(*dlgsubs->user_cb.on_tsx_state)(sub, tsx, event);
}


/*
* Called when SUBSCRIBE is received.
*/
static void dialog_on_evsub_rx_refresh( pjsip_evsub *sub, 
												 pjsip_rx_data *rdata,
												 int *p_st_code,
												 pj_str_t **p_st_text,
												 pjsip_hdr *res_hdr,
												 pjsip_msg_body **p_body)
{
	pjsip_dialogsub *dlgsubs;

	dlgsubs = (pjsip_dialogsub*) pjsip_evsub_get_mod_data(sub, mod_dialog.id);
	PJ_ASSERT_ON_FAIL(dlgsubs!=NULL, {return;});

	if (dlgsubs->user_cb.on_rx_refresh) {
		(*dlgsubs->user_cb.on_rx_refresh)(sub, rdata, p_st_code, p_st_text, res_hdr, p_body);

	} else {
		
	}
}

/*
* Process the content of incoming NOTIFY request and update temporary
* status.
*
* return PJ_SUCCESS if incoming request is acceptable. If return value
*	  is not PJ_SUCCESS, res_hdr may be added with Warning header.
*/
static pj_status_t dialog_process_rx_notify( pjsip_dialogsub *dlgsubs,
														pjsip_rx_data *rdata, 
														int *p_st_code,
														pj_str_t **p_st_text,
														pjsip_hdr *res_hdr)
{
	pjsip_ctype_hdr *ctype_hdr;
	pj_status_t status = PJ_SUCCESS;

	*p_st_text = NULL;

	/* Check Content-Type and msg body are present. */
	ctype_hdr = rdata->msg_info.ctype;

	if (ctype_hdr==NULL || rdata->msg_info.msg->body==NULL) {

		pjsip_warning_hdr *warn_hdr;
		pj_str_t warn_text;

		*p_st_code = PJSIP_SC_BAD_REQUEST;

		warn_text = pj_str("Message body is not present");
		warn_hdr = pjsip_warning_hdr_create(rdata->tp_info.pool, 399,
			pjsip_endpt_name(dlgsubs->dlg->endpt),
			&warn_text);
		pj_list_push_back(res_hdr, warn_hdr);

		return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
	}

	/* Parse content. */

	if (pj_stricmp(&ctype_hdr->media.type, &STR_APPLICATION)==0 &&
		pj_stricmp(&ctype_hdr->media.subtype, &STR_DIDF_XML)==0)
	{
		pjdidf_dialog_info *didf;

		didf = pjdidf_parse(rdata->tp_info.pool, (char*)rdata->msg_info.msg->body->data, rdata->msg_info.msg->body->len);
		if (didf == NULL) status = PJSIP_SIMPLE_EBADCIDF;
	}
	else
	{
		status = PJSIP_SIMPLE_EBADCONTENT;
	}

	if (status != PJ_SUCCESS) {
		/* Unsupported or bad Content-Type */
		pjsip_accept_hdr *accept_hdr;
		pjsip_warning_hdr *warn_hdr;

		*p_st_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;

		/* Add Accept header */
		accept_hdr = pjsip_accept_hdr_create(rdata->tp_info.pool);
		accept_hdr->values[accept_hdr->count++] = STR_APP_CIDF_XML;
		pj_list_push_back(res_hdr, accept_hdr);

		/* Add Warning header */
		warn_hdr = pjsip_warning_hdr_create_from_status(
			rdata->tp_info.pool,
			pjsip_endpt_name(dlgsubs->dlg->endpt),
			status);
		pj_list_push_back(res_hdr, warn_hdr);

		return status;
	}

	return PJ_SUCCESS;
}


/*
* Called when NOTIFY is received.
*/
static void dialog_on_evsub_rx_notify( pjsip_evsub *sub, 
												pjsip_rx_data *rdata,
												int *p_st_code,
												pj_str_t **p_st_text,
												pjsip_hdr *res_hdr,
												pjsip_msg_body **p_body)
{
	pjsip_dialogsub *dlgsubs;
	pj_status_t status;

	dlgsubs = (pjsip_dialogsub*) pjsip_evsub_get_mod_data(sub, mod_dialog.id);
	PJ_ASSERT_ON_FAIL(dlgsubs!=NULL, {return;});
		
	if (rdata->msg_info.msg->body) {
		status = dialog_process_rx_notify( dlgsubs, rdata, p_st_code, p_st_text, res_hdr );
		if (status != PJ_SUCCESS)
			return;

	} else {
		return;
	}

	/* Notify application. */
	if (dlgsubs->user_cb.on_rx_notify) {
		(*dlgsubs->user_cb.on_rx_notify)(sub, rdata, p_st_code, p_st_text, res_hdr, p_body);
	}

	/* Done */
}

/*
* Called when it's time to send SUBSCRIBE.
*/
static void dialog_on_evsub_client_refresh(pjsip_evsub *sub)
{
	pjsip_dialogsub *dlgsubs;

	dlgsubs = (pjsip_dialogsub*) pjsip_evsub_get_mod_data(sub, mod_dialog.id);
	PJ_ASSERT_ON_FAIL(dlgsubs!=NULL, {return;});

	if (dlgsubs->user_cb.on_client_refresh) {
		(*dlgsubs->user_cb.on_client_refresh)(sub);
	} else {
		pj_status_t status;
		pjsip_tx_data *tdata;

		status = pjsip_dialog_initiate(sub, -1, &tdata);
		if (status == PJ_SUCCESS)
			pjsip_dialog_send_request(sub, tdata);
	}
}

/*
* Called when no refresh is received after the interval.
*/
static void dialog_on_evsub_server_timeout(pjsip_evsub *sub)
{
	pjsip_dialogsub *dlgsubs;

	dlgsubs = (pjsip_dialogsub*) pjsip_evsub_get_mod_data(sub, mod_dialog.id);
	PJ_ASSERT_ON_FAIL(dlgsubs!=NULL, {return;});

	if (dlgsubs->user_cb.on_server_timeout) {
		(*dlgsubs->user_cb.on_server_timeout)(sub);
	} else {
		
	}
}

/*
* Function to print XML message body.
*/
static int xml_print_body(struct pjsip_msg_body *msg_body, char *buf, pj_size_t size)
{
	return pj_xml_print((const pj_xml_node*)msg_body->data, buf, size, PJ_TRUE);
}

/*
* Function to clone XML document.
*/
static void* xml_clone_data(pj_pool_t *pool, const void *data, unsigned len)
{
	PJ_UNUSED_ARG(len);
	return pj_xml_clone( pool, (const pj_xml_node*) data);
}

PJ_DEF(pjdidf_dialog_info*) pjdidf_create(pj_pool_t *pool, unsigned version, const pj_str_t *state, const pj_str_t *entity)
{
	pjdidf_dialog_info *conf = PJ_POOL_ALLOC_T(pool, pjdidf_dialog_info);
	if (conf == NULL) 
	{
		PJ_LOG(3,(__FILE__, "ERROR: pjdidf_create: NO HAY MEMORIA")); 
		return NULL;
	}
	pjdidf_dialog_info_construct(pool, conf, version, state, entity);
	return conf;
}

static void xml_init_node(pj_pool_t *pool, pj_xml_node *node, pj_str_t *name, const pj_str_t *value)
{
	pj_list_init(&node->attr_head);
	pj_list_init(&node->node_head);
	node->name = *name;
	if (value) pj_strdup(pool, &node->content, value);
	else node->content.ptr=NULL, node->content.slen=0;
}

static pj_xml_attr* xml_create_attr(pj_pool_t *pool, pj_str_t *name, const pj_str_t *value)
{
	pj_xml_attr *attr = PJ_POOL_ALLOC_T(pool, pj_xml_attr);
	attr->name = *name;
	pj_strdup(pool, &attr->value, value);
	return attr;
}

PJ_DEF(void) pjdidf_dialog_info_construct(pj_pool_t *pool, pjdidf_dialog_info *dialog_info, unsigned version, const pj_str_t *state, const pj_str_t *entity)
{
	pj_xml_attr *attr;
	char buff[25];
	pj_str_t ver;
	int i = 0;

	xml_init_node(pool, dialog_info, &DIALOG_INFO, NULL);
	attr = xml_create_attr(pool, &XMLNS, &DIDF_XMLNS);
	pj_xml_add_attr(dialog_info, attr);

	pj_utoa(version, buff);
	ver = pj_str(buff);

	attr = xml_create_attr(pool, &VERSION, &ver);
	pj_xml_add_attr(dialog_info, attr);

	if (state && state->slen)
	{
		attr = xml_create_attr(pool, &STATE, state);
		pj_xml_add_attr(dialog_info, attr);
	}

	attr = xml_create_attr(pool, &ENTITY, entity);
	pj_xml_add_attr(dialog_info, attr);	
}

PJ_DEF(unsigned) pjdidf_dialog_get_version(const pjdidf_dialog_info *dialog_info )
{
	const pj_xml_attr *attr = pj_xml_find_attr(dialog_info, &VERSION, NULL);
	if (attr)
		return pj_strtoul(&attr->value);

	return 0;
}

PJ_DEF(const pj_str_t*) pjdidf_dialog_get_state(const pjdidf_dialog_info *dialog_info )
{
	const pj_xml_attr *attr = pj_xml_find_attr(dialog_info, &STATE, NULL);
	if (attr)
		return &attr->value;

	return &FULL;
}

//Crea el nodo dialog y agrega los atributos
PJ_DEF(pj_xml_node *) pjdidf_dialog_add_dialog(pj_pool_t *pool, pjdidf_dialog_info *dialog_info, const pj_str_t *dialog_id, const pj_str_t *call_id, 
											  const pj_str_t *local_tag, const pj_str_t *remote_tag, const pj_str_t *direction)
{
	pj_xml_attr *attr;
	pj_xml_node *dialog_node;

	dialog_node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
	xml_init_node(pool, dialog_node, &DIALOG, NULL);
	pj_xml_add_node(dialog_info, dialog_node);

	if (dialog_id && dialog_id->slen)
	{
		attr = xml_create_attr(pool, &DIALOG_ID, dialog_id);
		pj_xml_add_attr(dialog_node, attr);
	}

	if (call_id && call_id->slen)
	{
		attr = xml_create_attr(pool, &CALL_ID, call_id);
		pj_xml_add_attr(dialog_node, attr);
	}

	if (local_tag && local_tag->slen)
	{
		attr = xml_create_attr(pool, &LOCAL_TAG, local_tag);
		pj_xml_add_attr(dialog_node, attr);
	}

	if (remote_tag && remote_tag->slen)
	{
		attr = xml_create_attr(pool, &REMOTE_TAG, remote_tag);
		pj_xml_add_attr(dialog_node, attr);
	}
	
	if (direction && direction->slen)
	{
		attr = xml_create_attr(pool, &DIRECTION, direction);
		pj_xml_add_attr(dialog_node, attr);
	}

	return dialog_node;
}

//Crea el nodo simple con un valor
PJ_DEF(pj_xml_node *) pjdidf_dialog_add_single_node(pj_pool_t *pool, pj_xml_node *dialog, const pj_str_t *name, const pj_str_t *value)
{	
	pj_xml_node *single_node = pj_xml_find_node(dialog, name);

	if (!single_node) {
		single_node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
		xml_init_node(pool, single_node, (pj_str_t *) name, NULL);
		pj_xml_add_node(dialog, single_node);
	}

	if (value && value->slen) pj_strdup(pool, &single_node->content, value);
	else single_node->content.ptr=NULL, single_node->content.slen=0;

	return single_node;
}

PJ_DEF(pj_xml_node *) pjdidf_dialog_add_state(pj_pool_t *pool, pj_xml_node *dialog, const pj_str_t *value)
{
	return pjdidf_dialog_add_single_node(pool, dialog, &STATE, value);
}

PJ_DEF(pj_xml_node *) pjdidf_dialog_add_duration(pj_pool_t *pool, pj_xml_node *dialog, const long value)
{
	char buff[25];
	pj_str_t dur;

	pj_utoa(value, buff);
	dur = pj_str(buff);

	return pjdidf_dialog_add_single_node(pool, dialog, &DURATION, &dur);
}

PJ_DEF(pj_xml_node *) pjdidf_dialog_add_local_remote(pj_pool_t *pool, pj_xml_node *dialog, const pj_str_t *local_remote)
{	
	pj_xml_node *node = pj_xml_find_node(dialog, local_remote);

	if (!node) {
		node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
		xml_init_node(pool, node, (pj_str_t *) local_remote, NULL);
		pj_xml_add_node(dialog, node);
	}
	
	return node;
}

PJ_DEF(pj_xml_node *) pjdidf_dialog_add_identity(pj_pool_t *pool, pj_xml_node *local_remote, pj_str_t *display, pj_str_t *uri)
{
	pj_xml_attr *attr;
	pj_xml_node *node = pj_xml_find_node(local_remote, &IDENTITY);

	if (!node) {
		node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
		xml_init_node(pool, node, &IDENTITY, NULL);
		pj_xml_add_node(local_remote, node);
	}

	if (display->ptr != NULL && display->slen != 0)
	{
		attr = xml_create_attr(pool, &DISPLAY, display);
		pj_xml_add_attr(node, attr);
	}

	pj_strdup(pool, &node->content, uri);
	
	return node;
}

PJ_DEF(pj_xml_node *) pjdidf_dialog_add_target(pj_pool_t *pool, pj_xml_node *local_remote, pj_str_t *uri)
{
	pj_xml_attr *attr;
	pj_xml_node *node = pj_xml_find_node(local_remote, &TARGET);

	if (!node) {
		node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
		xml_init_node(pool, node, &TARGET, NULL);
		pj_xml_add_node(local_remote, node);
	}

	attr = xml_create_attr(pool, &URI, uri);
	pj_xml_add_attr(node, attr);
	
	return node;
}

PJ_DEF(pj_xml_node *) pjdidf_dialog_add_param(pj_pool_t *pool, pj_xml_node *parent_node, pj_str_t *pname, pj_str_t *pval)
{
	pj_xml_attr *attr;
	pj_xml_node *node;
	pj_str_t PARAM = { "param", 5 };
	pj_str_t PNAME = { "pname", 5 };
	pj_str_t PVAL = { "pval", 4 };

	node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
	xml_init_node(pool, node, &PARAM, NULL);
	pj_xml_add_node(parent_node, node);

	attr = xml_create_attr(pool, &PNAME, pname);
	pj_xml_add_attr(node, attr);

	attr = xml_create_attr(pool, &PVAL, pval);
	pj_xml_add_attr(node, attr);
	
	return node;
}

PJ_DEF(pjdidf_dialog_info*) pjdidf_parse(pj_pool_t *pool, char *text, int len)
{
	pjdidf_dialog_info *dialog_info = pj_xml_parse(pool, text, len);
	if (dialog_info && dialog_info->name.slen >= DIALOG_INFO.slen) {
		pj_str_t name;

		name.ptr = dialog_info->name.ptr;
		name.slen = DIALOG_INFO.slen;

		if (pj_stricmp(&name, &DIALOG_INFO) == 0)
			return dialog_info;
	}

	return NULL;
}


