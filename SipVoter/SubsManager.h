#ifndef __CORESIP_SUBSMANAGER_H__
#define __CORESIP_SUBSMANAGER_H__

#include "Global.h"
#include "SipAgent.h"
#include "Exceptions.h"

//Clase para administrar las subscripciones (eventos conferencia, dialogo)

template <class T> class SubsManager
{

private:

	static const int MAX_SUBSCRIPTIONS = 1024;

	struct st_subscrip
	{
		char *dst;		//Destino que se monitoriza en la subscripcion
		char *user;		//Usuario de la uri de destino. Tanto user como domain nos servirá para identificar la subscripcion en 
						//este array. 
		char *domain;	//Dominio de la uri de destino.
		T *subs;		//Objeto que gestiona la subscripcion

	} subscriptions[MAX_SUBSCRIPTIONS];

	int subs_count;		//Número de subscripciones añadidas al array subcriptions

	pj_pool_t * _Pool;

	pj_mutex_t *mutex;

public:

	SubsManager()
	{
		for (int i = 0; i < MAX_SUBSCRIPTIONS; i++)
		{
			subscriptions[i].dst = NULL;
			subscriptions[i].subs = NULL;
		}
		subs_count = 0;

		_Pool = pjsua_pool_create(NULL, 256, 256);

		pj_status_t st = pj_mutex_create_simple(_Pool, "SubsManag_mutex", &mutex);
		PJ_CHECK_STATUS(st, ("ERROR: SubsManager: creando mutex"));

	}

	~SubsManager()
	{
		int i = 0;
		T *subs_to_delete = NULL;
		char *dst_to_delete = NULL;
		char *user_to_delete = NULL;
		char *domain_to_delete = NULL;

		//Se eliminan todas las subscripciones
		int scount = 0; //Cuenta los miembros de subscriptions activos
		pj_mutex_lock(mutex);
		while ((scount < subs_count) && (i < MAX_SUBSCRIPTIONS))
		{
			if (subscriptions[i].dst != NULL) 
			{
				user_to_delete = subscriptions[i].user;
				domain_to_delete = subscriptions[i].domain;
				subs_to_delete = subscriptions[i].subs;
				dst_to_delete = subscriptions[i].dst;
				pj_mutex_unlock(mutex);
				if (subs_to_delete != NULL)
				{
					subs_to_delete->End();
					delete subs_to_delete;
				}
				if (user_to_delete != NULL) free(user_to_delete);
				if (domain_to_delete != NULL) free(domain_to_delete);
				if (dst_to_delete != NULL) free(dst_to_delete);
				pj_mutex_lock(mutex);
				scount++;		
			}
			i++;
		}
		pj_mutex_unlock(mutex);

		if (_Pool)
		{
			pj_pool_release(_Pool);
		}
	}

	/**
	 * Add.	...
	 * Añade un destino al gestor de subscripciones 
	 * @param	acc_id				Account del agente que utilizaremos
	 * @param	dst					Uri al que enviamos la subscripcion al evento 
	 * @param	expires				en segundos, es el expires de la subscripcion. Si es -1 se utilizara el valor de por defecto
	 * @param	by_proxy. Si true entonces el subscribe se envia a traves del proxy
	 * @return	-1 si hay error.
	 */
	int Add(pjsua_acc_id acc_id, char *dst, int expires, pj_bool_t by_proxy)
	{
		pjsip_uri* uri;

		pj_pool_t *tmppool = pjsua_pool_create("SubsManager::Add", 64, 32);
		if (tmppool == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager::Add")); 	
			return -1;
		}

		/*Se crea un string duplicado para el parse, ya que se ha visto que
		pjsip_parse_uri puede modificar el parametro de entrada*/
		pj_str_t uri_aux;
		uri_aux.ptr = dst;
		uri_aux.slen = strlen(dst);
		pj_str_t uri_dup;
		pj_strdup_with_null(tmppool, &uri_dup, &uri_aux);
	
		uri = pjsip_parse_uri(tmppool, uri_dup.ptr, uri_dup.slen, 0);
		if (uri == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: No se puede crear objeto de la susbcripcion. Uri no valida dst %s", dst));
			pj_pool_release(tmppool);
			return -1;
		}

		/*Se extrae user y domain (host)*/
		pjsip_sip_uri *url=(pjsip_sip_uri*)pjsip_uri_get_uri(uri);

		char *new_user = (char *) malloc(url->user.slen+1);
		if (new_user == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager::Add")); 	
			pj_pool_release(tmppool);
			return -1;
		}
		memset(new_user, 0, url->user.slen+1);
		memcpy(new_user, url->user.ptr, url->user.slen);

		char *new_domain = (char *) malloc(url->host.slen+1);
		if (new_domain == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager::Add")); 
			free(new_user);
			pj_pool_release(tmppool);
			return -1;
		}
		memset(new_domain, 0, url->host.slen+1);
		memcpy(new_domain, url->host.ptr, url->host.slen);

		pj_pool_release(tmppool);

		char *new_dst = (char *) malloc(strlen(dst)+1);
		if (new_dst == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager::Add")); 	
			free(new_user);
			free(new_domain);		
			return -1;
		}
		strcpy(new_dst, dst);

		T *new_subs = new T(acc_id, dst, expires, by_proxy);	//Crea nueva subscripcion para un destino
		if (new_subs == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: No se puede crear objeto de la susbcripcion dst %s", dst));
			free(new_dst);
			free(new_user);
			free(new_domain);
			return -1;
		}		

		//Busca un elemento del array libre
		int i = 0;
		pj_bool_t ya_existe = PJ_FALSE;
		pj_bool_t no_libres = PJ_FALSE;
		pj_bool_t agregado_nuevo = PJ_FALSE;
		int new_index = MAX_SUBSCRIPTIONS;

		pj_mutex_lock(mutex);
		int scount = 0; //Cuenta los miembros de subscriptions activos
		while ((scount < subs_count) && (i < MAX_SUBSCRIPTIONS))
		{
			//Si i alcanza subs_count. Entonces ya se ha recorrido todos los elementos del array no libres
			//Si j alcanza MAX_SUBSCRIPTIONS. Entonces no hay huecos libres en el array

			if (subscriptions[i].dst != NULL) 
			{
				if ((strcmp(subscriptions[i].user, (const char *) new_user) == 0) &&
					(strcmp(subscriptions[i].domain, (const char *) new_domain) == 0))
				{
					ya_existe = PJ_TRUE;
					break;
				}	
				scount++;		
			}
			else
			{
				//Se ha encontrado un hueco libre en el array antes de que i alcance subs_count			
				new_index = i;
				break;
			}
			i++;
		}

		if (!ya_existe)
		{
			if (new_index < MAX_SUBSCRIPTIONS)
			{
				//Ha encontrado una posicion libre en el array. new_index sera donde se agregue el nuevo elemento			
			}
			else if ((new_index == MAX_SUBSCRIPTIONS) && (i < MAX_SUBSCRIPTIONS) && (scount == subs_count))
			{
				//No ha encontrado un hueco libre antes de que scount alcanzase a subs_count e 
				//i no ha alcanzado el final de array. Tomamos la siguiente posicion libre
				new_index = i;
			}
			else 
			{
				//No hay huecos libres en todo el array
				no_libres = PJ_TRUE;
			}

			if (new_index < MAX_SUBSCRIPTIONS)
			{
				subscriptions[new_index].subs = new_subs;					
				subscriptions[new_index].dst = new_dst;
				subscriptions[new_index].user = new_user;
				subscriptions[new_index].domain = new_domain;
				subs_count++;
				agregado_nuevo = PJ_TRUE;
			}
		}
		pj_mutex_unlock(mutex);

		if (agregado_nuevo)
		{
			//Se ha agregado un nuevo elemento. Lo inicializamos.
			//Le pasamos como paremetro una funcion cb y el puntero de el objeto
			if (new_subs->Init(SubsManager::SubsManager_Cb, (void *) this) == -1)
			{
				PJ_LOG(3,(__FILE__, "ERROR: Iniciando nueva subscripcion. dst %s", dst));
				delete new_subs;
				free(new_dst);
				free(new_user);
				free(new_domain);			

				pj_mutex_lock(mutex);
				subscriptions[new_index].subs = NULL;					
				subscriptions[new_index].dst = NULL;
				subscriptions[new_index].user = NULL;
				subscriptions[new_index].domain = NULL;
				if (subs_count > 0) subs_count--;
				pj_mutex_unlock(mutex);

				return -1;
			}
		}
		else
		{
			//Como no se ha abregado nuevo elemento. LIberamos la memoria usada		
			delete new_subs;
			free(new_dst);
			free(new_user);
			free(new_domain);
		}

		if (no_libres)
		{
			PJ_LOG(3,(__FILE__, "ERROR: No se puede añadir una nueva subscripcion porque se ha alcanzado el maximo numero. dst %s", dst));		
			return -1;
		}
	
		return 0;
	}

	/**
	 * Remove.	...
	 * Elimina un destino al gestor de subscripcion
	 * @param	dst			Uri del destino 
	 * @return	-1 si hay error.
	 */
	int Remove(char *dst)
	{
		int i = 0;
		pj_bool_t found = PJ_FALSE;
		T *subs_to_delete = NULL;
		char *dst_to_delete = NULL;
		char *user_to_delete = NULL;
		char *domain_to_delete = NULL;
		pjsip_uri* uri;

		pj_pool_t *tmppool = pjsua_pool_create("SubsManager::Remove", 64, 32);
		if (tmppool == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager::Remove")); 	
			return -1;
		}

		/*Se crea un string duplicado para el parse, ya que se ha visto que
		pjsip_parse_uri puede modificar el parametro de entrada*/
		pj_str_t uri_aux;
		uri_aux.ptr = dst;
		uri_aux.slen = strlen(dst);
		pj_str_t uri_dup;
		pj_strdup_with_null(tmppool, &uri_dup, &uri_aux);
	
		uri = pjsip_parse_uri(tmppool, uri_dup.ptr, uri_dup.slen, 0);
		if (uri == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: No se puede eliminar el objeto de la susbcripcion. Uri no valida dst %s", dst));
			pj_pool_release(tmppool);
			return -1;
		}

		/*Se extrae user y domain (host)*/
		pjsip_sip_uri *url=(pjsip_sip_uri*)pjsip_uri_get_uri(uri);

		char *rem_user = (char *) malloc(url->user.slen+1);
		if (rem_user == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager::Remove")); 	
			pj_pool_release(tmppool);
			return -1;
		}
		memset(rem_user, 0, url->user.slen+1);
		memcpy(rem_user, url->user.ptr, url->user.slen);

		char *rem_domain = (char *) malloc(url->host.slen+1);
		if (rem_domain == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager::Remove")); 
			free(rem_user);
			pj_pool_release(tmppool);
			return -1;
		}
		memset(rem_domain, 0, url->host.slen+1);
		memcpy(rem_domain, url->host.ptr, url->host.slen);

		pj_pool_release(tmppool);

		pj_mutex_lock(mutex);
		int scount = 0; //Cuenta los miembros de subscriptions activos
		while ((scount < subs_count) && (i < MAX_SUBSCRIPTIONS))
		{
			if (subscriptions[i].dst != NULL) 
			{
				if ((strcmp(subscriptions[i].user, (const char *) rem_user) == 0) &&
					(strcmp(subscriptions[i].domain, (const char *) rem_domain) == 0))
				{
					user_to_delete = subscriptions[i].user;
					subscriptions[i].user = NULL;
					domain_to_delete = subscriptions[i].domain;
					subscriptions[i].domain = NULL;
					subs_to_delete = subscriptions[i].subs;
					subscriptions[i].subs = NULL;
					dst_to_delete = subscriptions[i].dst;
					subscriptions[i].dst = NULL;
					found = PJ_TRUE;
					break;
				}	
				scount++;		
			}
			i++;
		}
		if (found) subs_count--;
		pj_mutex_unlock(mutex);

		free(rem_user);
		free(rem_domain);

		if (!found) 
		{
			PJ_LOG(3,(__FILE__, "WARNING: SubsManager::Remove: La Subscripcion no ha sido creada previamente o ya ha sido borrada. dst %s", dst));
			return 0;
		}
		else
		{
			if (subs_to_delete != NULL)
			{
				subs_to_delete->End();
				delete subs_to_delete;
			}
			if (user_to_delete != NULL) free(user_to_delete);
			if (domain_to_delete != NULL) free(domain_to_delete);
			if (dst_to_delete != NULL) free(dst_to_delete);		
		}

		return 0;
	}

	/**
	 * GetSubsObj.	...
	 * Retorna el objeto de la subscripcion activa 
	 * @param	dst			Uri del destino de la subscripcion
	 * @return	Puntero a un objeto. NULL si no lo encuentra
	 */
	T * GetSubsObj(char *dst)
	{
		int i = 0;
		pj_bool_t found = PJ_FALSE;
		T *subs_to_return = NULL;
		pjsip_uri* uri;

		pj_pool_t *tmppool = pjsua_pool_create("GetSubsObj", 64, 32);
		if (tmppool == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager<T>::GetSubsObj")); 	
			return NULL;
		}

		/*Se crea un string duplicado para el parse, ya que se ha visto que
		pjsip_parse_uri puede modificar el parametro de entrada*/
		pj_str_t uri_aux;
		uri_aux.ptr = dst;
		uri_aux.slen = strlen(dst);
		pj_str_t uri_dup;
		pj_strdup_with_null(tmppool, &uri_dup, &uri_aux);
	
		uri = pjsip_parse_uri(tmppool, uri_dup.ptr, uri_dup.slen, 0);
		if (uri == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: SubsManager<T>::GetSubsObj: Uri no valida dst %s", dst));
			pj_pool_release(tmppool);
			return NULL;
		}

		/*Se extrae user y domain (host)*/
		pjsip_sip_uri *url=(pjsip_sip_uri*)pjsip_uri_get_uri(uri);

		char *rem_user = (char *) malloc(url->user.slen+1);
		if (rem_user == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager<T>::GetSubsObj")); 	
			pj_pool_release(tmppool);
			return NULL;
		}
		memset(rem_user, 0, url->user.slen+1);
		memcpy(rem_user, url->user.ptr, url->user.slen);

		char *rem_domain = (char *) malloc(url->host.slen+1);
		if (rem_domain == NULL)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en SubsManager<T>::GetSubsObj")); 
			free(rem_user);
			pj_pool_release(tmppool);
			return NULL;
		}
		memset(rem_domain, 0, url->host.slen+1);
		memcpy(rem_domain, url->host.ptr, url->host.slen);

		pj_pool_release(tmppool);

		pj_mutex_lock(mutex);
		int scount = 0; //Cuenta los miembros de subscriptions activos
		while ((scount < subs_count) && (i < MAX_SUBSCRIPTIONS))
		{
			if (subscriptions[i].dst != NULL) 
			{
				if ((strcmp(subscriptions[i].user, (const char *) rem_user) == 0) &&
					(strcmp(subscriptions[i].domain, (const char *) rem_domain) == 0))
				{
					subs_to_return = subscriptions[i].subs;
					found = PJ_TRUE;
					break;
				}	
				scount++;		
			}
			i++;
		}
		pj_mutex_unlock(mutex);

		free(rem_user);
		free(rem_domain);

		if (!found) 
		{
			PJ_LOG(3,(__FILE__, "SubsManager<T>::GetSubsObj: No esta subscrito dst %s", dst));
		}

		return subs_to_return;
	}

	static void SubsManager_Cb(void *cs)
	{
		T *subs = (T *) cs;
		if (cs == NULL) return;

		if (subs->_Module == NULL)
		{	
			SubsManager *subsmanag = (SubsManager *) subs->_SubsManager;
			//La subscripcion ha sido terminada. La eliminamos.
			if (subsmanag != NULL)
			{
				subsmanag->Remove(subs->_Dst_uri);
			}
		}
	}




};



#endif