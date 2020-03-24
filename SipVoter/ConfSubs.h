#ifndef __CORESIP_CONFSUBS_H__
#define __CORESIP_CONFSUBS_H__

/*
En este modulo se implementa la clase que gestina una subscripcion al evento de conferencia
*/

#include "Global.h"
#include "exceptions.h"

typedef void (*ConfSubs_callback)(void *confsubs);

class ConfSubs
{
public:
	ConfSubs(pjsua_acc_id acc_id, char *dst_uri, int expires, pj_bool_t by_proxy);
	~ConfSubs();
	int Init(ConfSubs_callback confsub_cb, void *subsmanager);
	int End();
	int InicializarSubscripcion();

	char _Dst_uri[CORESIP_MAX_URI_LENGTH];		//Uri del destino al que enviamos la subscripcion al evento
	pjsip_evsub *_Module;
	void *_SubsManager;		//Puntero del objeto SubsManager que administra este sito de subscripciones.

	/*Funciones referentes a cuando funciona como servidor de subscripcion*/
	static pjsip_evsub_user _ConfSrvCb;	
	static void OnConfSrvStateChanged(pjsip_evsub *sub, pjsip_event *event);

private:
	
	static pjsip_evsub_user conf_callback;
	pj_pool_t *_Pool;		
	pj_str_t _Dst_uri_pj;
	pjsua_acc_id _Acc_id;						//Account del agente
	int _Expires;
	pjsip_dialog * _Dlg;						//Dialogo necesario para la subscripcion
	ConfSubs_callback _confsub_cb;
	pj_sem_t *status_sem;
	pj_bool_t _ByProxy;						//Indica si se envia el subscribe por el proxy

	static void conf_on_state(pjsip_evsub *sub, pjsip_event *event);
	static void conf_on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body);
	static void conf_on_client_refresh(pjsip_evsub *sub);

};

#endif