#ifndef __CORESIP_PRESSUBS_H__
#define __CORESIP_PRESSUBS_H__

class PresSubs
{
public:
	PresSubs(char *dst, void (*PresSubs_callback)(char *dst, int subscription_status, int presence_status, void *data), void *data);
	~PresSubs();

	int Init();
	int End();

	unsigned int nSubsTries;							//Numero de intentos para solicitar la subscripcion al evento de presencia
	unsigned int MAX_nSubsTries;						//Maximo numero de intentos
	static const unsigned int MAX_SUBS_TIME = 3000;		//Tiempo maximo de espera a que la subscripcion tenga exito en ms

private:	
	static pjsip_evsub_user presence_callback; 
	static enum subs_status
	{
		TERMINADA=0,
		ACTIVADA,
		REPOSO
	};

	void *_Data;
	pj_pool_t * _Pool;	
	pj_str_t _Dst;
	pjsip_evsub * _Module;

	//Callback para notificar el estado de presencia
	// dst. Uri del destino
	// subscription_status. Vale 1 si la subscripción está activa. 0 no activa
	// presence_status. Vale 1 si está presente. 0 si no lo está
	void (*_PresSubs_callback)(char *dst, int subscription_status, int presence_status, void *data);

	subs_status _SubscriptionStatus;			//True si la subscripción está activa
	pj_bool_t	_PresenceStatus;				//True si hay presencia

	pj_bool_t notify_after_initial;				//Si vale true entonces el siguiente notify a recibir es el primero despues de un SUBSCRIBE inicial
	pj_bool_t notify_after_refresh;				//Si vale true entonces el siguiente notify a recibir es el primero despues de un SUBSCRIBE de refresco
	char last_valid_tuple_id_buf[100];
	pj_str_t last_valid_tuple_id;
	char last_timestamp_buf[50];
	pj_str_t last_timestamp;

private:

	static void presence_on_state(pjsip_evsub *sub, pjsip_event *event);
	static void presence_on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body);
	static void presence_on_client_refresh(pjsip_evsub *sub);
};

#endif


