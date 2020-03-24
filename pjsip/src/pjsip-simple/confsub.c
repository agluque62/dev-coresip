#include <pjsip-simple/confsub.h>
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


#define THIS_FILE						"confsub.c"
#define CONF_DEFAULT_EXPIRES		PJSIP_CONF_DEFAULT_EXPIRES

/*
* Conference module (mod-conference)
*/
static struct pjsip_module mod_conference = 
{
	NULL, NULL,			    /* prev, next.			*/
	{ "mod-conference", 14 },	    /* Name.				*/
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
* Conference message body type.
*/
typedef enum content_type_e
{
	CONTENT_TYPE_NONE,
	CONTENT_TYPE_CIDF,
} content_type_e;

/*
* This structure describe a subscription, for both subscriber and notifier.
*/
struct pjsip_conf
{
	pjsip_evsub		*sub;		/**< Event subscribtion record.	    */
	pjsip_dialog	*dlg;		/**< The dialog.		    */
	content_type_e	 content_type;	/**< Content-Type.		    */
	pj_pool_t		*status_pool;	/**< Pool for conf_status	    */
	pjsip_conf_status	 status;	/**< Presence status.		    */
	pj_pool_t		*tmp_pool;	/**< Pool for tmp_status	    */
	pjsip_conf_status	 tmp_status;	/**< Temp, before NOTIFY is answred.*/
	pjsip_evsub_user	 user_cb;	/**< The user callback.		    */
};


typedef struct pjsip_conf pjsip_conf;


/*
* Forward decl for evsub callback.
*/
static void conf_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);
static void conf_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx, pjsip_event *event);
static void conf_on_evsub_rx_refresh( pjsip_evsub *sub, 
												 pjsip_rx_data *rdata,
												 int *p_st_code,
												 pj_str_t **p_st_text,
												 pjsip_hdr *res_hdr,
												 pjsip_msg_body **p_body);
static void conf_on_evsub_rx_notify( pjsip_evsub *sub, 
												pjsip_rx_data *rdata,
												int *p_st_code,
												pj_str_t **p_st_text,
												pjsip_hdr *res_hdr,
												pjsip_msg_body **p_body);
static void conf_on_evsub_client_refresh(pjsip_evsub *sub);
static void conf_on_evsub_server_timeout(pjsip_evsub *sub);

static int xml_print_body(struct pjsip_msg_body *msg_body, char *buf, pj_size_t size);
static void* xml_clone_data(pj_pool_t *pool, const void *data, unsigned len);

/*
* Event subscription callback for conference.
*/
static pjsip_evsub_user conf_user = 
{
	&conf_on_evsub_state,
	&conf_on_evsub_tsx_state,
	&conf_on_evsub_rx_refresh,
	&conf_on_evsub_rx_notify,
	&conf_on_evsub_client_refresh,
	&conf_on_evsub_server_timeout,
};


/*
* Some static constants.
*/
static const pj_str_t STR_EVENT				= { "Event", 5 };
static const pj_str_t STR_CONFERENCE		= { "conference", 10 };
static const pj_str_t STR_APPLICATION		= { "application", 11 };
static const pj_str_t STR_CIDF_XML			= { "conference-info+xml", 19};
static const pj_str_t STR_APP_CIDF_XML		= { "application/conference-info+xml", 31 };


/*
* Init conference module.
*/
PJ_DEF(pj_status_t) pjsip_conf_init_module( pjsip_endpoint *endpt)
{
	pj_status_t status;
	pj_str_t accept[1];

	/* Check arguments. */
	PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

	/* Must have not been registered */
	PJ_ASSERT_RETURN(mod_conference.id == -1, PJ_EINVALIDOP);

	/* Register to endpoint */
	status = pjsip_endpt_register_module(endpt, &mod_conference);
	if (status != PJ_SUCCESS)
		return status;

	accept[0] = STR_APP_CIDF_XML;

	/* Register event package to event module. */
	status = pjsip_evsub_register_pkg( &mod_conference, &STR_CONFERENCE, 
		CONF_DEFAULT_EXPIRES, 
		PJ_ARRAY_SIZE(accept), accept);
	if (status != PJ_SUCCESS) {
		pjsip_endpt_unregister_module(endpt, &mod_conference);
		return status;
	}

	return PJ_SUCCESS;
}


/*
* Get conference module instance.
*/
PJ_DEF(pjsip_module*) pjsip_conf_instance(void)
{
	return &mod_conference;
}


/*
* Create client subscription.
*/
PJ_DEF(pj_status_t) pjsip_conf_create_uac( pjsip_dialog *dlg,
														const pjsip_evsub_user *user_cb,
														unsigned options,
														pjsip_evsub **p_evsub )
{
	pj_status_t status;
	pjsip_conf *conf;
	char obj_name[PJ_MAX_OBJ_NAME];
	pjsip_evsub *sub;

	PJ_ASSERT_RETURN(dlg && p_evsub, PJ_EINVAL);

	pjsip_dlg_inc_lock(dlg);

	/* Create event subscription */
	status = pjsip_evsub_create_uac( dlg, &conf_user, &STR_CONFERENCE, options, &sub);
	if (status != PJ_SUCCESS)
		goto on_return;

	/* Create conference */
	conf = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_conf);
	conf->dlg = dlg;
	conf->sub = sub;
	if (user_cb)
		pj_memcpy(&conf->user_cb, user_cb, sizeof(pjsip_evsub_user));

	pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "conf%p", dlg->pool);
	conf->status_pool = pj_pool_create(dlg->pool->factory, obj_name, 512, 512, NULL);
	pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "tmpconf%p", dlg->pool);
	conf->tmp_pool = pj_pool_create(dlg->pool->factory, obj_name, 512, 512, NULL);

	/* Attach to evsub */
	pjsip_evsub_set_mod_data(sub, mod_conference.id, conf);

	*p_evsub = sub;

on_return:
	pjsip_dlg_dec_lock(dlg);
	return status;
}


/*
* Create server subscription.
*/
PJ_DEF(pj_status_t) pjsip_conf_create_uas( pjsip_dialog *dlg,
														const pjsip_evsub_user *user_cb,
														pjsip_rx_data *rdata,
														pjsip_evsub **p_evsub )
{
	pjsip_accept_hdr *accept;
	pjsip_event_hdr *event;
	content_type_e content_type = CONTENT_TYPE_NONE;
	pjsip_evsub *sub;
	pjsip_conf *conf;
	char obj_name[PJ_MAX_OBJ_NAME];
	pj_status_t status;

	/* Check arguments */
	PJ_ASSERT_RETURN(dlg && rdata && p_evsub, PJ_EINVAL);

	/* Must be request message */
	PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG, PJSIP_ENOTREQUESTMSG);

	/* Check that request is SUBSCRIBE */
	PJ_ASSERT_RETURN(pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
		&pjsip_subscribe_method)==0,
		PJSIP_SIMPLE_ENOTSUBSCRIBE);

	/* Check that Event header contains "conference" */
	event = (pjsip_event_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_EVENT, NULL);
	if (!event) {
		return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
	}
	if (pj_stricmp(&event->event_type, &STR_CONFERENCE) != 0) {
		return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_EVENT);
	}

	/* Check that request contains compatible Accept header. */
	accept = (pjsip_accept_hdr*)pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_ACCEPT, NULL);
	if (accept) {
		unsigned i;
		for (i=0; i<accept->count; ++i) {
			if (pj_stricmp(&accept->values[i], &STR_APP_CIDF_XML)==0) {
				content_type = CONTENT_TYPE_CIDF;
				break;
			} 
		}

		if (i==accept->count) {
			/* Nothing is acceptable */
			return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE);
		}

	} else {
		/* No Accept header.
		* Treat as "application/conference-info+xml"
		*/
		content_type = CONTENT_TYPE_CIDF;
	}

	/* Lock dialog */
	pjsip_dlg_inc_lock(dlg);

	/* Create server subscription */
	status = pjsip_evsub_create_uas( dlg, &conf_user, rdata, 0, &sub);
	if (status != PJ_SUCCESS)
		goto on_return;

	/* Create server conference subscription */
	conf = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_conf);
	conf->dlg = dlg;
	conf->sub = sub;
	conf->content_type = content_type;
	if (user_cb)
		pj_memcpy(&conf->user_cb, user_cb, sizeof(pjsip_evsub_user));

	pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "conf%p", dlg->pool);
	conf->status_pool = pj_pool_create(dlg->pool->factory, obj_name, 512, 512, NULL);
	pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "tmpconf%p", dlg->pool);
	conf->tmp_pool = pj_pool_create(dlg->pool->factory, obj_name, 512, 512, NULL);

	/* Attach to evsub */
	pjsip_evsub_set_mod_data(sub, mod_conference.id, conf);

	/* Done: */
	*p_evsub = sub;

on_return:
	pjsip_dlg_dec_lock(dlg);
	return status;
}


/*
* Forcefully terminate conference subscription.
*/
PJ_DEF(pj_status_t) pjsip_conf_terminate( pjsip_evsub *sub, pj_bool_t notify )
{
	return pjsip_evsub_terminate(sub, notify);
}

/*
* Create SUBSCRIBE
*/
PJ_DEF(pj_status_t) pjsip_conf_initiate( pjsip_evsub *sub, pj_int32_t expires, pjsip_tx_data **p_tdata)
{
	return pjsip_evsub_initiate(sub, &pjsip_subscribe_method, expires, p_tdata);
}

/*
* Accept incoming subscription.
*/
PJ_DEF(pj_status_t) pjsip_conf_accept( pjsip_evsub *sub, pjsip_rx_data *rdata, int st_code, const pjsip_hdr *hdr_list )
{
	return pjsip_evsub_accept( sub, rdata, st_code, hdr_list );
}

/*
* Get conference status.
*/
PJ_DEF(pj_status_t) pjsip_conf_get_status( pjsip_evsub *sub, pjsip_conf_status *status )
{
	pjsip_conf *conf;

	PJ_ASSERT_RETURN(sub && status, PJ_EINVAL);

	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_RETURN(conf!=NULL, PJSIP_SIMPLE_ENOCONFERENCE);

	if (conf->tmp_status._is_valid) {
		PJ_ASSERT_RETURN(conf->tmp_pool!=NULL, PJSIP_SIMPLE_ENOCONFERENCE);
		pj_memcpy(status, &conf->tmp_status, sizeof(pjsip_conf_status));
	} else {
		PJ_ASSERT_RETURN(conf->status_pool!=NULL, PJSIP_SIMPLE_ENOCONFERENCE);
		pj_memcpy(status, &conf->status, sizeof(pjsip_conf_status));
	}

	return PJ_SUCCESS;
}

/*
* Set conference status.
*/
PJ_DEF(pj_status_t) pjsip_conf_set_status( pjsip_evsub *sub, const pjsip_conf_status *status )
{
	unsigned i;
	pj_pool_t *tmp;
	pjsip_conf *conf;

	PJ_ASSERT_RETURN(sub && status, PJ_EINVAL);

	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_RETURN(conf!=NULL, PJSIP_SIMPLE_ENOCONFERENCE);

	for (i=0; i<status->users_cnt; ++i) {
		pj_strdup(conf->tmp_pool, &conf->status.users[i].id, &status->users[i].id);
		pj_strdup(conf->tmp_pool, &conf->status.users[i].display, &status->users[i].display);
		pj_strdup(conf->tmp_pool, &conf->status.users[i].role, &status->users[i].role);
	}

	conf->status.users_cnt = status->users_cnt;
	conf->status.version = status->version;
	pj_strdup(conf->tmp_pool, &conf->status.state, &status->state);


	/* Swap pools */
	tmp = conf->tmp_pool;
	conf->tmp_pool = conf->status_pool;
	conf->status_pool = tmp;
	pj_pool_reset(conf->tmp_pool);

	return PJ_SUCCESS;
}

/*
* Create message body.
*/
static pj_status_t conf_create_msg_body( pjsip_conf *conf, pjsip_tx_data *tdata)
{
	pj_str_t entity;

	/* Get publisher URI */
	entity.ptr = (char*) pj_pool_alloc(tdata->pool, PJSIP_MAX_URL_SIZE);
	entity.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
		conf->dlg->local.info->uri,
		entity.ptr, PJSIP_MAX_URL_SIZE);
	if (entity.slen < 1)
		return PJ_ENOMEM;

	if (conf->content_type == CONTENT_TYPE_CIDF) {

		return pjsip_conf_create_cidf(tdata->pool, &conf->status, &entity, &tdata->msg->body);

	} else {
		return PJSIP_SIMPLE_EBADCONTENT;
	}
}

/*
* Create NOTIFY
*/
PJ_DEF(pj_status_t) pjsip_conf_notify( pjsip_evsub *sub,
												  pjsip_evsub_state state,
												  const pj_str_t *state_str,
												  const pj_str_t *reason,
												  pjsip_tx_data **p_tdata)
{
	pjsip_conf *conf;
	pjsip_tx_data *tdata;
	pj_status_t status;

	/* Check arguments. */
	PJ_ASSERT_RETURN(sub, PJ_EINVAL);

	/* Get the conference object. */
	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_RETURN(conf != NULL, PJSIP_SIMPLE_ENOCONFERENCE);

	/* Must have conference info valid, unless state is 
	* PJSIP_EVSUB_STATE_TERMINATED. This could happen if subscription
	* has not been active (e.g. we're waiting for user authorization)
	* and remote cancels the subscription.
	*/
	PJ_ASSERT_RETURN(state==PJSIP_EVSUB_STATE_TERMINATED ||
		conf->status.version > 0, PJSIP_SIMPLE_ENOCONFERENCEINFO);

	/* Lock object. */
	pjsip_dlg_inc_lock(conf->dlg);

	/* Create the NOTIFY request. */
	status = pjsip_evsub_notify( sub, state, state_str, reason, &tdata);
	if (status != PJ_SUCCESS)
		goto on_return;


	/* Create message body to reflect the conference status. 
	* Only do this if we have conference info to send (see above).
	*/
	if (conf->status.version > 0) {
		status = conf_create_msg_body( conf, tdata );
		if (status != PJ_SUCCESS)
			goto on_return;
	}

	/* Done. */
	*p_tdata = tdata;


on_return:
	pjsip_dlg_dec_lock(conf->dlg);
	return status;
}

/*
* Create NOTIFY that reflect current state.
*/
PJ_DEF(pj_status_t) pjsip_conf_current_notify( pjsip_evsub *sub, pjsip_tx_data **p_tdata )
{
	pjsip_conf *conf;
	pjsip_tx_data *tdata;
	pj_status_t status;

	/* Check arguments. */
	PJ_ASSERT_RETURN(sub, PJ_EINVAL);

	/* Get the presence object. */
	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_RETURN(conf != NULL, PJSIP_SIMPLE_ENOCONFERENCE);

	/* We may not have a conference info yet, e.g. when we receive SUBSCRIBE
	* to refresh subscription while we're waiting for user authorization.
	*/
	//PJ_ASSERT_RETURN(conf->status.version > 0, 
	//		       PJSIP_SIMPLE_ENOCONFERENCEINFO);


	/* Lock object. */
	pjsip_dlg_inc_lock(conf->dlg);

	/* Create the NOTIFY request. */
	status = pjsip_evsub_current_notify( sub, &tdata);
	if (status != PJ_SUCCESS)
		goto on_return;

	/* Create message body to reflect the presence status. */
	if (conf->status.version > 0) {
		status = conf_create_msg_body( conf, tdata );
		if (status != PJ_SUCCESS)
			goto on_return;
	}

	/* Done. */
	*p_tdata = tdata;

on_return:
	pjsip_dlg_dec_lock(conf->dlg);
	return status;
}

/*
* Send request.
*/
PJ_DEF(pj_status_t) pjsip_conf_send_request( pjsip_evsub *sub, pjsip_tx_data *tdata )
{
	return pjsip_evsub_send_request(sub, tdata);
}

PJ_DEF(pj_status_t) pjsip_conf_create_cidf( pj_pool_t *pool,
														  const pjsip_conf_status *status,
														  const pj_str_t *entity,
														  pjsip_msg_body **p_body )
{
	pjcidf_conf *cidf;
	pjsip_msg_body *body;
	unsigned i;

	/* Create <conference-info>. */
	cidf = pjcidf_create(pool, entity, status->version, &status->state);

	/* Create <user> */
	for (i=0; i<status->users_cnt; ++i) {
		pjcidf_conf_add_user(pool, cidf, &status->users[i].id, &status->users[i].display, &status->users[i].role);
	}

	body = PJ_POOL_ZALLOC_T(pool, pjsip_msg_body);
	body->data = cidf;
	body->content_type.type = STR_APPLICATION;
	body->content_type.subtype = STR_CIDF_XML;
	body->print_body = &xml_print_body;
	body->clone_data = &xml_clone_data;

	*p_body = body;

	return PJ_SUCCESS;    
}


PJ_DEF(pj_status_t) pjsip_conf_parse_cidf(pjsip_rx_data *rdata,
														 pj_pool_t *pool,
														 pjsip_conf_status *status)
{
	pjcidf_conf *cidf;
	pjcidf_user *cidf_user;

	cidf = pjcidf_parse(rdata->tp_info.pool, 
		(char*)rdata->msg_info.msg->body->data,
		rdata->msg_info.msg->body->len);
	if (cidf == NULL)
		return PJSIP_SIMPLE_EBADCIDF;

	status->version = pjcidf_conf_get_version(cidf);
	pj_strdup(pool, &status->state, pjcidf_conf_get_state(cidf));
	
	status->users_cnt = 0;

	cidf_user = pjcidf_conf_get_first_user(cidf);
	while (cidf_user && status->users_cnt < PJSIP_CONF_STATUS_MAX_INFO) {
		pj_strdup(pool, &status->users[status->users_cnt].id, pjcidf_user_get_id(cidf_user));
		pj_strdup(pool, &status->users[status->users_cnt].display, pjcidf_user_get_display(cidf_user));
		pj_strdup(pool, &status->users[status->users_cnt].role, pjcidf_user_get_role(cidf_user));
		pj_strdup(pool, &status->users[status->users_cnt].state, pjcidf_user_get_state(cidf_user));

		cidf_user = pjcidf_conf_get_next_user( cidf, cidf_user );
		status->users_cnt++;
	}

	return PJ_SUCCESS;
}

/*
* This callback is called by event subscription when subscription
* state has changed.
*/
static void conf_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
	pjsip_conf *conf;

	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_ON_FAIL(conf!=NULL, {return;});

	if (conf->user_cb.on_evsub_state)
		(*conf->user_cb.on_evsub_state)(sub, event);

	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
		if (conf->status_pool) {
			pj_pool_release(conf->status_pool);
			conf->status_pool = NULL;
		}
		if (conf->tmp_pool) {
			pj_pool_release(conf->tmp_pool);
			conf->tmp_pool = NULL;
		}
	}
}

/*
* Called when transaction state has changed.
*/
static void conf_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx, pjsip_event *event)
{
	pjsip_conf *conf;

	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_ON_FAIL(conf!=NULL, {return;});

	if (conf->user_cb.on_tsx_state)
		(*conf->user_cb.on_tsx_state)(sub, tsx, event);
}


/*
* Called when SUBSCRIBE is received.
*/
static void conf_on_evsub_rx_refresh( pjsip_evsub *sub, 
												 pjsip_rx_data *rdata,
												 int *p_st_code,
												 pj_str_t **p_st_text,
												 pjsip_hdr *res_hdr,
												 pjsip_msg_body **p_body)
{
	pjsip_conf *conf;

	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_ON_FAIL(conf!=NULL, {return;});

	if (conf->user_cb.on_rx_refresh) {
		(*conf->user_cb.on_rx_refresh)(sub, rdata, p_st_code, p_st_text, res_hdr, p_body);

	} else {
		/* Implementors MUST send NOTIFY if it implements on_rx_refresh */
		pjsip_tx_data *tdata;
		pj_str_t timeout = { "timeout", 7};
		pj_status_t status;

		if (pjsip_evsub_get_state(sub)==PJSIP_EVSUB_STATE_TERMINATED) {
			status = pjsip_conf_notify( sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, &timeout, &tdata);
		} else {
			status = pjsip_conf_current_notify(sub, &tdata);
		}

		if (status == PJ_SUCCESS)
			pjsip_conf_send_request(sub, tdata);
	}
}

/*
* Process the content of incoming NOTIFY request and update temporary
* status.
*
* return PJ_SUCCESS if incoming request is acceptable. If return value
*	  is not PJ_SUCCESS, res_hdr may be added with Warning header.
*/
static pj_status_t conf_process_rx_notify( pjsip_conf *conf,
														pjsip_rx_data *rdata, 
														int *p_st_code,
														pj_str_t **p_st_text,
														pjsip_hdr *res_hdr)
{
	pjsip_ctype_hdr *ctype_hdr;
	pj_status_t status;

	*p_st_text = NULL;

	/* Check Content-Type and msg body are present. */
	ctype_hdr = rdata->msg_info.ctype;

	if (ctype_hdr==NULL || rdata->msg_info.msg->body==NULL) {

		pjsip_warning_hdr *warn_hdr;
		pj_str_t warn_text;

		*p_st_code = PJSIP_SC_BAD_REQUEST;

		warn_text = pj_str("Message body is not present");
		warn_hdr = pjsip_warning_hdr_create(rdata->tp_info.pool, 399,
			pjsip_endpt_name(conf->dlg->endpt),
			&warn_text);
		pj_list_push_back(res_hdr, warn_hdr);

		return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
	}

	/* Parse content. */

	if (pj_stricmp(&ctype_hdr->media.type, &STR_APPLICATION)==0 &&
		pj_stricmp(&ctype_hdr->media.subtype, &STR_CIDF_XML)==0)
	{
		status = pjsip_conf_parse_cidf( rdata, conf->tmp_pool, &conf->tmp_status);
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
			pjsip_endpt_name(conf->dlg->endpt),
			status);
		pj_list_push_back(res_hdr, warn_hdr);

		return status;
	}

	/* If application calls conf_get_status(), redirect the call to
	* retrieve the temporary status.
	*/
	conf->tmp_status._is_valid = PJ_TRUE;

	return PJ_SUCCESS;
}


/*
* Called when NOTIFY is received.
*/
static void conf_on_evsub_rx_notify( pjsip_evsub *sub, 
												pjsip_rx_data *rdata,
												int *p_st_code,
												pj_str_t **p_st_text,
												pjsip_hdr *res_hdr,
												pjsip_msg_body **p_body)
{
	pjsip_conf *conf;
	pj_status_t status;

	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_ON_FAIL(conf!=NULL, {return;});

	if (rdata->msg_info.msg->body) {
		status = conf_process_rx_notify( conf, rdata, p_st_code, p_st_text, res_hdr );
		if (status != PJ_SUCCESS)
			return;

	} else {
		return;
	}

	/* Notify application. */
	if (conf->user_cb.on_rx_notify) {
		(*conf->user_cb.on_rx_notify)(sub, rdata, p_st_code, p_st_text, res_hdr, p_body);
	}


	/* If application responded NOTIFY with 2xx, copy temporary status
	* to main status, and mark the temporary status as invalid.
	*/
	if ((*p_st_code)/100 == 2) {
		pj_pool_t *tmp;

		pj_memcpy(&conf->status, &conf->tmp_status, sizeof(pjsip_conf_status));

		/* Swap the pool */
		tmp = conf->tmp_pool;
		conf->tmp_pool = conf->status_pool;
		conf->status_pool = tmp;
	}

	conf->tmp_status._is_valid = PJ_FALSE;
	pj_pool_reset(conf->tmp_pool);

	/* Done */
}

/*
* Called when it's time to send SUBSCRIBE.
*/
static void conf_on_evsub_client_refresh(pjsip_evsub *sub)
{
	pjsip_conf *conf;

	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_ON_FAIL(conf!=NULL, {return;});

	if (conf->user_cb.on_client_refresh) {
		(*conf->user_cb.on_client_refresh)(sub);
	} else {
		pj_status_t status;
		pjsip_tx_data *tdata;

		status = pjsip_conf_initiate(sub, -1, &tdata);
		if (status == PJ_SUCCESS)
			pjsip_conf_send_request(sub, tdata);
	}
}

/*
* Called when no refresh is received after the interval.
*/
static void conf_on_evsub_server_timeout(pjsip_evsub *sub)
{
	pjsip_conf *conf;

	conf = (pjsip_conf*) pjsip_evsub_get_mod_data(sub, mod_conference.id);
	PJ_ASSERT_ON_FAIL(conf!=NULL, {return;});

	if (conf->user_cb.on_server_timeout) {
		(*conf->user_cb.on_server_timeout)(sub);
	} else {
		pj_status_t status;
		pjsip_tx_data *tdata;
		pj_str_t reason = { "timeout", 7 };

		status = pjsip_conf_notify(sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, &reason, &tdata);
		if (status == PJ_SUCCESS)
			pjsip_conf_send_request(sub, tdata);
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
