#ifndef __PJSIP_SIMPLE_DIALOG_H__
#define __PJSIP_SIMPLE_DIALOG_H__

/**
* @file confsub.h
* @brief SIP Extension for Dialog subscription 
*/
#include <pjsip-simple/evsub.h>
#include <pjsip-simple/cidf.h>


PJ_BEGIN_DECL


/**
* @defgroup PJSIP_SIMPLE_DLG SIP Extension for Dialog Subscription 
* @ingroup PJSIP_SIMPLE
* @brief Support for SIP Extension for Dialog Subscription 
* @{
*
* This module contains the implementation of SIP Dialog Subscription Extension as 
* described in RFC 4235. It uses the SIP Event Notification framework
* (evsub.h) and extends the framework by implementing "dialog"
* event package.
*/



/**
* Initialize the dialog module and register it as endpoint module and
* package to the event subscription module.
*
* @param endpt		The endpoint instance.
* @param mod_evsub	The event subscription module instance.
*
* @return		PJ_SUCCESS if the module is successfully 
*			initialized and registered to both endpoint
*			and the event subscription module.
*/
PJ_DECL(pj_status_t) pjsip_dialog_init_module(pjsip_endpoint *endpt);


/**
* Get the dialog module instance.
*
* @return		The dialog module instance.
*/
PJ_DECL(pjsip_module*) pjsip_dialog_instance(void);


/**
* Maximum dialog status users.
*/
#define PJSIP_DIALOG_STATUS_MAX_INFO  25


/**
* Create dialog client subscription session.
*
* @param dlg		The underlying dialog to use.
* @param user_cb	Pointer to callbacks to receive dialog subscription
*			events.
* @param options	Option flags. Currently only PJSIP_EVSUB_NO_EVENT_ID
*			is recognized.
* @param p_evsub	Pointer to receive the dialog subscription
*			session.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_dialog_create_uac( pjsip_dialog *dlg,
														 const pjsip_evsub_user *user_cb,
														 unsigned options,
														 pjsip_evsub **p_evsub );


/**
* Create dialog server subscription session.
*
* @param dlg		The underlying dialog to use.
* @param user_cb	Pointer to callbacks to receive dialog subscription
*			events.
* @param rdata		The incoming SUBSCRIBE request that creates the event 
*			subscription.
* @param p_evsub	Pointer to receive the dialog subscription
*			session.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_dialog_create_uas( pjsip_dialog *dlg,
														 const pjsip_evsub_user *user_cb,
														 pjsip_rx_data *rdata,
														 pjsip_evsub **p_evsub );


/**
* Forcefully destroy the dialog subscription. This function should only
* be called on special condition, such as when the subscription 
* initialization has failed. For other conditions, application MUST terminate
* the subscription by sending the appropriate un(SUBSCRIBE) or NOTIFY.
*
* @param sub		The dialog subscription.
* @param notify	Specify whether the state notification callback
*			should be called.
*
* @return		PJ_SUCCESS if subscription session has been destroyed.
*/
PJ_DECL(pj_status_t) pjsip_dialog_terminate( pjsip_evsub *sub,
														pj_bool_t notify );



/**
* Call this function to create request to initiate dialog subscription, to 
* refresh subcription, or to request subscription termination.
*
* @param sub		Client subscription instance.
* @param expires	Subscription expiration. If the value is set to zero,
*			this will request unsubscription.
* @param p_tdata	Pointer to receive the request.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_dialog_initiate( pjsip_evsub *sub,
													  pj_int32_t expires,
													  pjsip_tx_data **p_tdata);



/**
* Accept the incoming subscription request by sending 2xx response to
* incoming SUBSCRIBE request.
*
* @param sub		Server subscription instance.
* @param rdata		The incoming subscription request message.
* @param st_code	Status code, which MUST be final response.
* @param hdr_list	Optional list of headers to be added in the response.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_dialog_accept( pjsip_evsub *sub,
													pjsip_rx_data *rdata,
													int st_code,
													const pjsip_hdr *hdr_list );




/**
* For notifier, create NOTIFY request to subscriber, and set the state 
* of the subscription. Application MUST set the presence status to the
* appropriate state (by calling #pjsip_dialog_set_status()) before calling
* this function.
*
* @param callid. Call id de la llamada a la que wse refiere el dialogo sobre el que se envia la notificacion
* @param sub		The server subscription (notifier) instance.
* @param state		New state to set.
* @param state_str	The state string name, if state contains value other
*			than active, pending, or terminated. Otherwise this
*			argument is ignored.
* @param reason	Specify reason if new state is terminated, otherwise
*			put NULL.
* @param with_body. Includes body in the notify
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_dialog_notify( pjsua_call_id callid, pjsip_evsub *sub,
													pjsip_evsub_state state,
													const pj_str_t *state_str,
													const pj_str_t *reason,
													pj_bool_t with_body);

/**
* Create NOTIFY request to reflect current subscription status and send.
* @param callid. Call id de la llamada a la que wse refiere el dialogo sobre el que se envia la notificacion
* @param sub		Server subscription object.
* @param with_body. Includes body in the notify
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_dialog_current_notify( pjsua_call_id callid, pjsip_evsub *sub, pj_bool_t with_body);


/**
* Send request message that was previously created with initiate(), notify(),
* or current_notify(). Application may also send request created with other
* functions, e.g. authentication. But the request MUST be either request
* that creates/refresh subscription or NOTIFY request.
*
* @param sub		The subscription object.
* @param tdata		Request message to be sent.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_dialog_send_request( pjsip_evsub *sub,
															pjsip_tx_data *tdata );


/**
 * @defgroup PJSIP_SIMPLE_DIDF DIDF/Dialog Information Data Format (RFC 4235)
 * @ingroup PJSIP_SIMPLE
 * @brief Support for DIDF/Dialog Information Data Format (RFC 4235)
 * @{
 *
 * This file provides tools for manipulating Dialog Information Data 
 * Format (DIDF) as described in RFC 4235.
 */
typedef struct pj_xml_node pjdidf_dialog_info;

/******************************************************************************
 * Top level API for managing presence document. 
 *****************************************************************************/
PJ_DECL(pjdidf_dialog_info*) pjdidf_create(pj_pool_t *pool, unsigned version, const pj_str_t *state, const pj_str_t *entity);
PJ_DECL(pjdidf_dialog_info*) pjdidf_parse(pj_pool_t *pool, char *text, int len);


/******************************************************************************
 * API for managing Dialog node.
 *****************************************************************************/
PJ_DECL(void) pjdidf_dialog_info_construct(pj_pool_t *pool, pjdidf_dialog_info *dialog_info, unsigned version, const pj_str_t *state, const pj_str_t *entity);

PJ_DECL(unsigned) pjdidf_dialog_get_version(const pjdidf_dialog_info *dialog_info );
PJ_DECL(const pj_str_t*) pjdidf_dialog_get_state(const pjdidf_dialog_info *dialog_info );

PJ_DECL(pj_xml_node *)	 pjdidf_dialog_add_dialog(pj_pool_t *pool, pjdidf_dialog_info *dialog_info, const pj_str_t *dialog_id, const pj_str_t *call_id, 
											  const pj_str_t *local_tag, const pj_str_t *remote_tag, const pj_str_t *direction);
PJ_DECL(pj_xml_node *) pjdidf_dialog_add_state(pj_pool_t *pool, pj_xml_node *dialog, const pj_str_t *state);
PJ_DECL(pj_xml_node *) pjdidf_dialog_add_single_node(pj_pool_t *pool, pj_xml_node *dialog, const pj_str_t *name, const pj_str_t *value);
PJ_DECL(pj_xml_node *) pjdidf_dialog_add_state(pj_pool_t *pool, pj_xml_node *dialog, const pj_str_t *value);
PJ_DECL(pj_xml_node *) pjdidf_dialog_add_duration(pj_pool_t *pool, pj_xml_node *dialog, const long value);
PJ_DECL(pj_xml_node *) pjdidf_dialog_add_local_remote(pj_pool_t *pool, pj_xml_node *dialog, const pj_str_t *local_remote);
PJ_DECL(pj_xml_node *) pjdidf_dialog_add_identity(pj_pool_t *pool, pj_xml_node *local_remote, pj_str_t *display, pj_str_t *uri);
PJ_DECL(pj_xml_node *) pjdidf_dialog_add_target(pj_pool_t *pool, pj_xml_node *local_remote, pj_str_t *uri);
PJ_DECL(pj_xml_node *) pjdidf_dialog_add_param(pj_pool_t *pool, pj_xml_node *parent_node, pj_str_t *pname, pj_str_t *pval);
PJ_DECL(pjdidf_dialog_info*) pjdidf_parse(pj_pool_t *pool, char *text, int len);



/**
* @}
*/

PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_DIALOG_H__ */
