
#ifndef __CORESIP_PRESENCEMANAG_H__
#define __CORESIP_PRESENCEMANAG_H__

#include "Global.h"
#include "PresSubs.h"

class PresenceManag
{

public:	
	PresenceManag();
	~PresenceManag();

	int SetPresenceSubscriptionCallBack(void (*PresSubs_callback)(char *dst_uri, int subscription_status, int presence_status));
	int Add(char *dst);
	int Remove(char *dst);

private:

	static const int MAX_SUBSCRIPTIONS = 1024;

	struct st_subscrip
	{
		char *dst;		//Destino que se monitoriza en la subscripcion
		char *user;		//Usuario de la uri de destino. Tanto user como domain nos servirá para identificar la subscripcion en 
						//este array. 
		char *domain;	//Dominio de la uri de destino.
		PresSubs *subs;	//Objeto que gestiona la subscripcion

	} subscriptions[MAX_SUBSCRIPTIONS];

	int subs_count;		//Número de subscripciones añadidas al array subcriptions
	
	pj_pool_t * _Pool;

	pj_mutex_t *mutex;	

	/*Callback para notificar estados a la aplicacion*/
	SubPresCb _Presence_callback;

	/*Callback para recibir notificaciones de estado del objeto del tipo PresSubs*/
	static void PresSubs_callback(char *dst, int subscription_status, int presence_status, void *data);
	
};

#endif