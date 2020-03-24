#ifndef __PJSIP_SIMPLE_CONFERENCE_H__
#define __PJSIP_SIMPLE_CONFERENCE_H__

/**
* @file confsub.h
* @brief SIP Extension for Conference subscription (RFC 4575)
*/
#include <pjsip-simple/evsub.h>
#include <pjsip-simple/cidf.h>


PJ_BEGIN_DECL


/**
* @defgroup PJSIP_SIMPLE_CONF SIP Extension for Conference Subscription (RFC 4575)
* @ingroup PJSIP_SIMPLE
* @brief Support for SIP Extension for Conference Subscription (RFC 4575)
* @{
*
* This module contains the implementation of SIP Conference Subscription Extension as 
* described in RFC 4575. It uses the SIP Event Notification framework
* (evsub.h) and extends the framework by implementing "conference"
* event package.
*/



/**
* Initialize the conference module and register it as endpoint module and
* package to the event subscription module.
*
* @param endpt		The endpoint instance.
* @param mod_evsub	The event subscription module instance.
*
* @return		PJ_SUCCESS if the module is successfully 
*			initialized and registered to both endpoint
*			and the event subscription module.
*/
PJ_DECL(pj_status_t) pjsip_conf_init_module(pjsip_endpoint *endpt);


/**
* Get the conference module instance.
*
* @return		The conference module instance.
*/
PJ_DECL(pjsip_module*) pjsip_conf_instance(void);


/**
* Maximum conference status users.
*/
#define PJSIP_CONF_STATUS_MAX_INFO  25

/**
* This structure describes conference status.
*/
struct pjsip_conf_status
{
	unsigned		version;
	pj_str_t		state;
	unsigned		users_cnt;	/**< Number of users in the status.  */

	struct {

		pj_str_t	id;		/**< User id.			    */
		pj_str_t	display;	/**< Optional user-friendly name.	    */
		pj_str_t role;		/**< Optional user role						 */
		pj_str_t state;	/**< Optional user state valid only on rx */

	} users[PJSIP_CONF_STATUS_MAX_INFO];	/**< Array of users.		    */

	pj_bool_t		_is_valid;	/**< Internal flag.		    */
};


/**
* @see pjsip_conf_status
*/
typedef struct pjsip_conf_status pjsip_conf_status;


/**
* Create conference client subscription session.
*
* @param dlg		The underlying dialog to use.
* @param user_cb	Pointer to callbacks to receive conference subscription
*			events.
* @param options	Option flags. Currently only PJSIP_EVSUB_NO_EVENT_ID
*			is recognized.
* @param p_evsub	Pointer to receive the conference subscription
*			session.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_conf_create_uac( pjsip_dialog *dlg,
														 const pjsip_evsub_user *user_cb,
														 unsigned options,
														 pjsip_evsub **p_evsub );


/**
* Create conference server subscription session.
*
* @param dlg		The underlying dialog to use.
* @param user_cb	Pointer to callbacks to receive conference subscription
*			events.
* @param rdata		The incoming SUBSCRIBE request that creates the event 
*			subscription.
* @param p_evsub	Pointer to receive the conference subscription
*			session.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_conf_create_uas( pjsip_dialog *dlg,
														 const pjsip_evsub_user *user_cb,
														 pjsip_rx_data *rdata,
														 pjsip_evsub **p_evsub );


/**
* Forcefully destroy the conference subscription. This function should only
* be called on special condition, such as when the subscription 
* initialization has failed. For other conditions, application MUST terminate
* the subscription by sending the appropriate un(SUBSCRIBE) or NOTIFY.
*
* @param sub		The conference subscription.
* @param notify	Specify whether the state notification callback
*			should be called.
*
* @return		PJ_SUCCESS if subscription session has been destroyed.
*/
PJ_DECL(pj_status_t) pjsip_conf_terminate( pjsip_evsub *sub,
														pj_bool_t notify );



/**
* Call this function to create request to initiate conference subscription, to 
* refresh subcription, or to request subscription termination.
*
* @param sub		Client subscription instance.
* @param expires	Subscription expiration. If the value is set to zero,
*			this will request unsubscription.
* @param p_tdata	Pointer to receive the request.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_conf_initiate( pjsip_evsub *sub,
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
PJ_DECL(pj_status_t) pjsip_conf_accept( pjsip_evsub *sub,
													pjsip_rx_data *rdata,
													int st_code,
													const pjsip_hdr *hdr_list );




/**
* For notifier, create NOTIFY request to subscriber, and set the state 
* of the subscription. Application MUST set the presence status to the
* appropriate state (by calling #pjsip_conf_set_status()) before calling
* this function.
*
* @param sub		The server subscription (notifier) instance.
* @param state		New state to set.
* @param state_str	The state string name, if state contains value other
*			than active, pending, or terminated. Otherwise this
*			argument is ignored.
* @param reason	Specify reason if new state is terminated, otherwise
*			put NULL.
* @param p_tdata	Pointer to receive the request.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_conf_notify( pjsip_evsub *sub,
													pjsip_evsub_state state,
													const pj_str_t *state_str,
													const pj_str_t *reason,
													pjsip_tx_data **p_tdata);


/**
* Create NOTIFY request to reflect current subscription status.
*
* @param sub		Server subscription object.
* @param p_tdata	Pointer to receive request.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_conf_current_notify( pjsip_evsub *sub,
															  pjsip_tx_data **p_tdata );



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
PJ_DECL(pj_status_t) pjsip_conf_send_request( pjsip_evsub *sub,
															pjsip_tx_data *tdata );


/**
* Get the conference status. Client normally would call this function
* after receiving NOTIFY request from server.
*
* @param sub		The client or server subscription.
* @param status	The structure to receive conference status.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_conf_get_status( pjsip_evsub *sub,
														 pjsip_conf_status *status );


/**
* Set the conference status. This operation is only valid for server
* subscription. After calling this function, application would need to
* send NOTIFY request to client.
*
* @param sub		The server subscription.
* @param status	Status to be set.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_conf_set_status( pjsip_evsub *sub,
														 const pjsip_conf_status *status );


/**
* This is a utility function to create CIDF message body from PJSIP
* conference status (pjsip_conf_status).
*
* @param pool		The pool to allocate memory for the message body.
* @param status	Conference status to be converted into CIDF message
*			body.
* @param entity	The entity ID, which normally is equal to the 
*			conference ID publishing this conference info.
* @param p_body	Pointer to receive the SIP message body.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_conf_create_cidf( pj_pool_t *pool,
														  const pjsip_conf_status *status,
														  const pj_str_t *entity,
														  pjsip_msg_body **p_body );


/**
* This is a utility function to parse CIDF body into PJSIP conference status.
*
* @param rdata		The incoming SIP message containing the CIDF body.
* @param pool		Pool to allocate memory to copy the strings into
*			the conference status structure.
* @param status	The conference status to be initialized.
*
* @return		PJ_SUCCESS on success.
*/
PJ_DECL(pj_status_t) pjsip_conf_parse_cidf(pjsip_rx_data *rdata,
														 pj_pool_t *pool,
														 pjsip_conf_status *status);




/**
* @}
*/

PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_CONFERENCE_H__ */
