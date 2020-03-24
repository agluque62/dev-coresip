#ifndef __CORESIP_DLGSUBS_H__
#define __CORESIP_DLGSUBS_H__

/*
En este modulo se implementa la clase que gestiona una subscripcion al evento de dialogo
*/

#include "Global.h"
#include "exceptions.h"
#include "dlgsub.h"

typedef void (*DlgSubs_callback)(void *dlgsubs);

class DlgSubs
{
public:

	static const pj_bool_t TERMINATE = PJ_TRUE;
	static const pj_bool_t WITH_BODY = PJ_TRUE;

	DlgSubs(pjsua_acc_id acc_id, char *dst_uri, int expires, pj_bool_t by_proxy);
	~DlgSubs();
	int Init(DlgSubs_callback dlgsub_cb, void *subsmanager);
	int End();
	int InicializarSubscripcion();

	char _Dst_uri[CORESIP_MAX_URI_LENGTH];		//Uri del destino al que enviamos la subscripcion al evento
	pjsip_evsub *_Module;
	void *_SubsManager;		//Puntero del objeto SubsManager que administra este sito de subscripciones.

	/*Funciones referentes a cuando funciona como servidor de subscripcion*/
	static void OnDlgSrvStateChanged(pjsip_evsub *sub, pjsip_event *event);
	static void OnDlgSrvTsxChanged(pjsip_evsub *sub, pjsip_transaction *tsx, pjsip_event *event);
	static void OnDlgSrvRxRefresh( pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code,
			   pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body);
	static void OnDlgSrvTimeout(pjsip_evsub *sub);

	static pjsip_evsub_user _DlgSrvCb;

	static pj_status_t Current_notify_all_dialogs( pjsip_evsub *sub, pj_bool_t terminate, pj_bool_t with_body);
	static pj_status_t Notify_all_dialogs( pjsip_evsub *sub, pjsip_evsub_state state, const pj_str_t *state_str, const pj_str_t *reason, pj_bool_t with_body);

private:
	static pjsip_evsub_user dlg_callback;
	pj_pool_t *_Pool;
	pjsip_dialog * _Dlg;						//Dialogo necesario para la subscripcion
	pj_str_t _Dst_uri_pj;
	pjsua_acc_id _Acc_id;						//Account del agente
	DlgSubs_callback _dlgsub_cb;
	pj_sem_t *status_sem;
	int _Expires;
	pj_bool_t _ByProxy;						//Indica si se envia el subscribe por el proxy

	static void on_state(pjsip_evsub *sub, pjsip_event *event);
	static void on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body);
	static void on_client_refresh(pjsip_evsub *sub);
		
};

#endif