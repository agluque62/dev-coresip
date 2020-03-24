/**
 * @file ExtraParamAccId.h
 *
 * Define la clase del objeto que se asigna como user data a un account id
 */
#ifndef __EXTRAPARAMACCID_H__
#define __EXTRAPARAMACCID_H__

#include "Global.h"
#include "exceptions.h"
#include <list>

/*Estructura que define parametros extra de un account ID.*/
class ExtraParamAccId
{
private:

	struct from_user_subs
	{
		char user[CORESIP_MAX_USER_ID_LENGTH];
		char host[CORESIP_MAX_URI_LENGTH];
		char tag[CORESIP_MAX_TAG_LENGTH];
	};

	pj_pool_t *_Pool;		
	pjsip_evsub * _ConfSrvEvSub;		//Modulo para la la subscripcion al evento de converencia fuera de un dialogo INV
	std::list<pjsip_evsub *, std::allocator<pjsip_evsub *>> _subModlist;
										//Contiene los punteros a los modulos de subscripcion 
										//correspondiente a un account del agente

	std::list<from_user_subs *, std::allocator<from_user_subs *>> _DeletedsubModlist;
										//Contiene los punteros a los usuarios incluyendo su tag que han solicitado 
										//subscripcion y han sido borrados,
										//correspondientes a un account del agente. 
										//Sirve para cuando se recibe tarde un reintento de peticion de subscripcion 
										//con el mismo from pero que ya ha sido borrado de la lista _subModlist
	static const size_t MAX_DeletedsubModlist_size = 64;
										//Maxima cantidad de elementos de la lista _DeletedsubModlist. 
										//Si se alcanza esta cantidad entonces se elimina el elemento mas antiguo

	pj_mutex_t *_subModlist_mutex;

public:

	pj_bool_t rdAccount;				//Indica si es un account tipo radio. Se pone a true cuando es llamado por el ETM 
										//con la funcion CORESIP_SetTipoGRS.
										//Tambien lo pondremos a true si el agente el del tipo Radio_UA
	CORESIP_CallFlags TipoGrsFlags;

	//Estructura para el user_data de un modulo de subscripcion
	struct subs_user_data
	{
		pjsua_acc_id accid;			//Account del agente sobre el que se realiza la subscripcion
		pj_bool_t send_notify_at_tsx_terminate;
									//Indica que cuando a terminado la transaccion del metodo subscribe envie el notify		
	};

	ExtraParamAccId();
	~ExtraParamAccId();

	static int Add_subMod(pjsip_evsub *conf);
	static int Del_subMod(pjsip_evsub *conf);
	static pjsip_evsub * Get_subMod(pjsua_acc_id accid, pjsip_uri *remote_uri, pj_str_t *event_type);	
	static int Add_DeletedsubModlist(pjsua_acc_id accid, pjsip_uri *remote_uri, pj_str_t *remote_from_tag);
	static pj_bool_t IsInDeletedList(pjsua_acc_id accid, pjsip_uri *remote_uri, pj_str_t *remote_from_tag);
	static void SendConfInfoFromAcc(pjsua_acc_id accid, const CORESIP_ConfInfo * conf);
	static void SendDialogNotifyFromAcc(pjsua_call_id call_id, pj_bool_t with_body);
};

#endif