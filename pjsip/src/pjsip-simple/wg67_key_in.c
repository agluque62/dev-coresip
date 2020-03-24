#include <pjsip-simple/wg67_key_in.h>
#include <pjsip-simple/errno.h>
#include <pjsip-simple/evsub_msg.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_dialog.h>
#include <pjsip/sip_parser.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/except.h>

#define THIS_FILE						"wg67_key_in.c"
#define WG67_DEFAULT_EXPIRES		600

#define IS_NEWLINE(c)				((c)=='\r' || (c)=='\n')
#define IS_SPACE(c)					((c)==' ' || (c)=='\t')

/*
* WG67 KEY-IN module (mod-wg67)
*/
static struct pjsip_module mod_wg67 = 
{
	NULL, NULL,			    /* prev, next.			*/
	{ "mod-wg67", 8 },	    /* Name.				*/
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
* This structure describe a WG67 KEY-IN subscription
*/
struct pjsip_wg67
{
	pjsip_evsub *sub;					/**< Event subscribtion record.	    */
	pjsip_dialog *dlg;					/**< The dialog.		    */
	pjsip_wg67_status status;			/**< WG67 KEY-IN status.		    */
	pjsip_wg67_status tmp_status;	/**< Temp, before NOTIFY is answred.*/
	pjsip_evsub_user user_cb;			/**< The user callback.		    */
};


typedef struct pjsip_wg67 pjsip_wg67;


/*
* Forward decl for evsub callback.
*/
static void wg67_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);
static void wg67_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx,
												pjsip_event *event);
static void wg67_on_evsub_rx_notify( pjsip_evsub *sub, 
												pjsip_rx_data *rdata,
												int *p_st_code,
												pj_str_t **p_st_text,
												pjsip_hdr *res_hdr,
												pjsip_msg_body **p_body);
static void wg67_on_evsub_client_refresh(pjsip_evsub *sub);

pjsip_uri *  int_parse_uri_or_name_addr( pj_scanner *scanner, 
													 pj_pool_t *pool, 
													 unsigned option);

/*
* Event subscription callback for WG67 KEY-IN.
*/
static pjsip_evsub_user wg67_user = 
{
	&wg67_on_evsub_state,
	&wg67_on_evsub_tsx_state,
	NULL,
	&wg67_on_evsub_rx_notify,
	&wg67_on_evsub_client_refresh,
	NULL,
};


/*
* Some static constants.
*/
static const pj_str_t STR_EVENT = { "Event", 5 };
static const pj_str_t STR_WG67 = { "WG67KEY-IN", 10 };
static const pj_str_t STR_TEXT = { "text", 4 };
static const pj_str_t STR_PLAIN = { "plain", 5 };
static const pj_str_t STR_TEXT_PLAIN = { "text/plain", 10 };


/*
* Init WG67 KEY-IN module.
*/
PJ_DEF(pj_status_t) pjsip_wg67_init_module( pjsip_endpoint *endpt)
{
	pj_status_t status;
	pj_str_t accept[1];

	/* Check arguments. */
	PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

	/* Must have not been registered */
	PJ_ASSERT_RETURN(mod_wg67.id == -1, PJ_EINVALIDOP);

	/* Register to endpoint */
	status = pjsip_endpt_register_module(endpt, &mod_wg67);
	if (status != PJ_SUCCESS)
		return status;

	accept[0] = STR_TEXT_PLAIN;

	/* Register event package to event module. */
	status = pjsip_evsub_register_pkg( &mod_wg67, &STR_WG67, 
		WG67_DEFAULT_EXPIRES, 
		PJ_ARRAY_SIZE(accept), accept);
	if (status != PJ_SUCCESS) {
		pjsip_endpt_unregister_module(endpt, &mod_wg67);
		return status;
	}

	return PJ_SUCCESS;
}


/*
* Get WG67 KEY-IN module instance.
*/
PJ_DEF(pjsip_module*) pjsip_wg67_instance(void)
{
	return &mod_wg67;
}


/*
* Create client subscription.
*/
PJ_DEF(pj_status_t) pjsip_wg67_create_uac( pjsip_dialog *dlg,
														const pjsip_evsub_user *user_cb,
														unsigned options,
														pjsip_evsub **p_evsub )
{
	pj_status_t status;
	pjsip_wg67 *wg67;
	pjsip_evsub *sub;

	PJ_ASSERT_RETURN(dlg && p_evsub, PJ_EINVAL);

	pjsip_dlg_inc_lock(dlg);

	/* Create event subscription */
	status = pjsip_evsub_create_uac( dlg,  &wg67_user, &STR_WG67, 
		options, &sub);
	if (status != PJ_SUCCESS)
		goto on_return;

	/* Create WG67 KEY-IN */
	wg67 = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_wg67);
	wg67->dlg = dlg;
	wg67->sub = sub;
	if (user_cb)
		pj_memcpy(&wg67->user_cb, user_cb, sizeof(pjsip_evsub_user));

	/* Attach to evsub */
	pjsip_evsub_set_mod_data(sub, mod_wg67.id, wg67);

	*p_evsub = sub;

on_return:
	pjsip_dlg_dec_lock(dlg);
	return status;
}


/*
* Forcefully terminate WG67 KEY-IN.
*/
PJ_DEF(pj_status_t) pjsip_wg67_terminate( pjsip_evsub *sub,
													  pj_bool_t notify )
{
	return pjsip_evsub_terminate(sub, notify);
}


/*
* Create SUBSCRIBE
*/
PJ_DEF(pj_status_t) pjsip_wg67_initiate( pjsip_evsub *sub,
													 pj_int32_t expires,
													 pjsip_tx_data **p_tdata)
{
	return pjsip_evsub_initiate(sub, &pjsip_subscribe_method, expires, 
		p_tdata);
}


/*
* Get WG67 KEY-IN status.
*/
PJ_DEF(pj_status_t) pjsip_wg67_get_status( pjsip_evsub *sub,
														pjsip_wg67_status *status )
{
	pjsip_wg67 *wg67;

	PJ_ASSERT_RETURN(sub && status, PJ_EINVAL);

	wg67 = (pjsip_wg67*) pjsip_evsub_get_mod_data(sub, mod_wg67.id);
	PJ_ASSERT_RETURN(wg67!=NULL, PJSIP_SIMPLE_ENOWG67);

	if (wg67->tmp_status._is_valid)
		pj_memcpy(status, &wg67->tmp_status, sizeof(pjsip_wg67_status));
	else
		pj_memcpy(status, &wg67->status, sizeof(pjsip_wg67_status));

	return PJ_SUCCESS;
}


/*
* Send request.
*/
PJ_DEF(pj_status_t) pjsip_wg67_send_request( pjsip_evsub *sub,
														  pjsip_tx_data *tdata )
{
	return pjsip_evsub_send_request(sub, tdata);
}


/*
* This callback is called by event subscription when subscription
* state has changed.
*/
static void wg67_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
	pjsip_wg67 *wg67;

	wg67 = (pjsip_wg67*) pjsip_evsub_get_mod_data(sub, mod_wg67.id);
	PJ_ASSERT_ON_FAIL(wg67!=NULL, {return;});

	if (wg67->user_cb.on_evsub_state)
		(*wg67->user_cb.on_evsub_state)(sub, event);
}

/*
* Called when transaction state has changed.
*/
static void wg67_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx,
												pjsip_event *event)
{
	pjsip_wg67 *wg67;

	wg67 = (pjsip_wg67*) pjsip_evsub_get_mod_data(sub, mod_wg67.id);
	PJ_ASSERT_ON_FAIL(wg67!=NULL, {return;});

	if (wg67->user_cb.on_tsx_state)
		(*wg67->user_cb.on_tsx_state)(sub, tsx, event);
}


/*
* Process the content of incoming NOTIFY request and update temporary
* status.
*
* return PJ_SUCCESS if incoming request is acceptable. If return value
*	  is not PJ_SUCCESS, res_hdr may be added with Warning header.
*/
static pj_status_t wg67_process_rx_notify( pjsip_wg67 *wg67,
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
			pjsip_endpt_name(wg67->dlg->endpt),
			&warn_text);
		pj_list_push_back(res_hdr, warn_hdr);

		return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
	}

	/* Parse content. */

	if (pj_stricmp(&ctype_hdr->media.type, &STR_TEXT)==0 &&
		pj_stricmp(&ctype_hdr->media.subtype, &STR_PLAIN)==0)
	{
		status = pjsip_wg67_parse_body( rdata, wg67->dlg->pool,
			&wg67->tmp_status);
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
		accept_hdr->values[accept_hdr->count++] = STR_TEXT_PLAIN;
		pj_list_push_back(res_hdr, accept_hdr);

		/* Add Warning header */
		warn_hdr = pjsip_warning_hdr_create_from_status(
			rdata->tp_info.pool,
			pjsip_endpt_name(wg67->dlg->endpt),
			status);
		pj_list_push_back(res_hdr, warn_hdr);

		return status;
	}

	/* If application calls wg67_get_status(), redirect the call to
	* retrieve the temporary status.
	*/
	wg67->tmp_status._is_valid = PJ_TRUE;

	return PJ_SUCCESS;
}


/*
* Called when NOTIFY is received.
*/
static void wg67_on_evsub_rx_notify( pjsip_evsub *sub, 
												pjsip_rx_data *rdata,
												int *p_st_code,
												pj_str_t **p_st_text,
												pjsip_hdr *res_hdr,
												pjsip_msg_body **p_body)
{
	pjsip_wg67 *wg67;
	pj_status_t status;

	wg67 = (pjsip_wg67*) pjsip_evsub_get_mod_data(sub, mod_wg67.id);
	PJ_ASSERT_ON_FAIL(wg67!=NULL, {return;});

	if (rdata->msg_info.msg->body) {
		status = wg67_process_rx_notify( wg67, rdata, p_st_code, p_st_text,
			res_hdr );
		if (status != PJ_SUCCESS)
			return;

	} else {
		/* 
		* We treat it as no change in WG67 KEY-IN status
		*/
		*p_st_code = 200;
		return;
	}

	/* Notify application. */
	if (wg67->user_cb.on_rx_notify) {
		(*wg67->user_cb.on_rx_notify)(sub, rdata, p_st_code, p_st_text, 
			res_hdr, p_body);
	}


	/* If application responded NOTIFY with 2xx, copy temporary status
	* to main status, and mark the temporary status as invalid.
	*/
	if ((*p_st_code)/100 == 2) {
		pj_memcpy(&wg67->status, &wg67->tmp_status, sizeof(pjsip_wg67_status));
	}

	wg67->tmp_status._is_valid = PJ_FALSE;

	/* Done */
}


/*
* Called when it's time to send SUBSCRIBE.
*/
static void wg67_on_evsub_client_refresh(pjsip_evsub *sub)
{
	pjsip_wg67 *wg67;

	wg67 = (pjsip_wg67*) pjsip_evsub_get_mod_data(sub, mod_wg67.id);
	PJ_ASSERT_ON_FAIL(wg67!=NULL, {return;});

	if (wg67->user_cb.on_client_refresh) {
		(*wg67->user_cb.on_client_refresh)(sub);
	} else {
		pj_status_t status;
		pjsip_tx_data *tdata;

		status = pjsip_wg67_initiate(sub, -1, &tdata);
		if (status == PJ_SUCCESS)
			pjsip_wg67_send_request(sub, tdata);
	}
}


/* Syntax error handler for parser. */
static void on_syntax_error(pj_scanner *scanner)
{
	PJ_UNUSED_ARG(scanner);
	PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
}


PJ_DEF(pj_status_t) pjsip_wg67_parse_body(pjsip_rx_data *rdata,
														 pj_pool_t *pool,
														 pjsip_wg67_status *status)
{
	pj_scanner scanner;
	pj_status_t ret = PJSIP_SIMPLE_EBADWG67;
	PJ_USE_EXCEPTION;

	pj_scan_init(&scanner, (char*)rdata->msg_info.msg->body->data, 
		rdata->msg_info.msg->body->len, 0, &on_syntax_error);

	// PlugTest FAA 05/2011
	// Las info de WG67 se iban acumulando...
	status->info_cnt=0;
	PJ_TRY 
	{
		/* Skip leading newlines. */
		while (IS_NEWLINE(*scanner.curptr)) 
		{
			pj_scan_get_newline(&scanner);
		}

		while (!pj_scan_is_eof(&scanner) && !IS_NEWLINE(*scanner.curptr) && 
			(status->info_cnt < PJSIP_WG67_STATUS_MAX_INFO)) 
		{
			pj_str_t pttid,sessiontype;
			pjsip_uri * uri;

			pj_scan_get_until_ch(&scanner, ',', &pttid);			// Obtiene PttId
			pj_scan_get_char(&scanner);								// Salta la coma.

			uri = int_parse_uri_or_name_addr(&scanner, pool, PJSIP_PARSE_URI_AS_NAMEADDR);
			pj_scan_get_char(&scanner);								// Salta la coma.

			pj_scan_get_until_chr(&scanner, "\r\n", &sessiontype);		// Obtiene el STRING del Tipo de Session.
			pj_scan_get_newline(&scanner);							// Salta el Fin de Linea...

			status->info[status->info_cnt].pttid = (unsigned short)pj_strtoul(&pttid);
			pjsip_name_addr_assign(pool, &(status->info[status->info_cnt].sipid), (pjsip_name_addr *)uri);
			status->info_cnt++;
		}

		ret = PJ_SUCCESS;
	}
	PJ_CATCH_ANY 
	{
		PJ_LOG(4, (THIS_FILE, "Error parsing WG67 KEY-IN body in line %d col %d",
			scanner.line, pj_scan_get_col(&scanner)));
	}
	PJ_END;

	pj_scan_fini(&scanner);
	return ret;
}
