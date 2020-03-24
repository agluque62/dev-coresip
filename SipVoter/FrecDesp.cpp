/**
 * @file FrecDesp.h
 * @brief Gestion de Frecuencias Desplazadas en CORESIP.dll
 *
 *	Implementa la clase 'FrecDesp'.
 *
 *	@addtogroup CORESIP
 */
/*@{*/

#include "Global.h"
#include "Exceptions.h"
#include "SipAgent.h"
#include "FrecDesp.h"

#define STRCMP(A,B) strcmp(A,B)
//#define STRCMP(A,B) strncmp(A, B, 5)	//Se considera del mismo grupo todas las radios que coincidan los 5 primeros caracteres
										//Sirve para probar climax antes de implementarlos en el NODEBOX

const float FrecDesp::OFFSET_THRESHOLD = 1.0;

/**
 * FrecDesp.	...
 * Constructor. Inicializa la lista de grupos.
 * @return	nada.
 */
FrecDesp::FrecDesp()
{
	for (int i = 0; i < MAX_GROUPS; i++)
	{
		groups[i].RdFr[0] = 0;
		for (int j = 0; j < MAX_SESSIONS; j++)
		{
			groups[i].sessions[j].sess_callid = PJSUA_INVALID_ID;
			groups[i].sessions[j].pSipcall = NULL;
			groups[i].sessions[j].TdTxIP = INVALID_TIME_DELAY;
			groups[i].sessions[j].Tj1 = INVALID_TIME_DELAY;
			groups[i].sessions[j].Tj1_count = 0;
			groups[i].sessions[j].Tn1_count = 0;
			groups[i].sessions[j].cld_absoluto = PJ_FALSE;
			groups[i].sessions[j].Bss_value = 0;
			groups[i].sessions[j].Bss_type = 0;
			groups[i].sessions[j].squ_status = PJ_FALSE;
			groups[i].sessions[j].selected = PJ_FALSE;
			groups[i].sessions[j].Tred = 0;
			groups[i].sessions[j]._MetodoClimax = Relative;
			groups[i].sessions[j].Bss_selected_method[0] = '\0';
			groups[i].sessions[j].Tid_orig = 0;
			groups[i].sessions[j].cld_prev = INVALID_CLD_PREV;
			groups[i].sessions[j].Flags = CORESIP_CALL_RD_RXONLY;
			groups[i].sessions[j].in_window_timer = PJ_FALSE;			
		}
		groups[i].nsessions = 0;
		groups[i].nsessions_rx_only = 0;
		groups[i].nsessions_tx_only = 0;
		groups[i].mcast_seq = 0;
		groups[i]._RdSendTo = NULL;
		groups[i].SelectedUri[0] = '\0';
		groups[i].SelectedUriPttId = 0;
	}
	ngroups = 0;
	NTP_synchronized = PJ_FALSE;

	_Pool = pjsua_pool_create(NULL, 1024, 1024);

	pj_status_t st = pj_mutex_create_simple(_Pool, "fd_mutex", &fd_mutex);
	PJ_CHECK_STATUS(st, ("ERROR creando mutex fd_mutex"));


	//Thread para chequear sincronizaci�n NTP	
	ntp_check_thread = NULL;
	ntp_check_thread_run = PJ_FALSE;

	ntp_sd = PJ_INVALID_SOCKET;
	/* initialise file descriptor set */
	PJ_FD_ZERO(&ntp_fds);

	ntp_check_thread_run = PJ_TRUE;
	st = pj_thread_create(_Pool, "NTPCheckTh", &NTPCheckTh, this, 0, 0, &ntp_check_thread);
	PJ_CHECK_STATUS(st, ("ERROR creando thread para chequear sincronizacion NTP"));
}


/**
 * ~FrecDesp.	...
 * Destructor.
 * @return	nada.
 */
FrecDesp::~FrecDesp()
{
	if (ntp_check_thread != NULL)
	{
		ntp_check_thread_run = PJ_FALSE;
		pj_thread_join(ntp_check_thread);
		pj_thread_destroy(ntp_check_thread);
		ntp_check_thread = NULL;
	}

	if (_Pool)
	{
		pj_pool_release(_Pool);
	}

}

/**
 * AddToGroup.	...
 * Agrega el call id de una sesion a un grupo. Si no existe un grupo con esa frecuencia, lo crea.
 * @param	rdfr		Identificador de la Frecuencia
 * @param   call_id		call id que se suma al grupo
 * @param   psipcall	Puntero del objeto SipCall que maneja la sesion
 * @param   Bss_selected_method	Metodo de bss
 * @param   flags		Flags de la sesion
 * @return	Numero de sesiones en el grupo. -1 si hay error y no se ha podido agregar al grupo o crearlo.
 */
int FrecDesp::AddToGroup(char *rdfr, pjsua_call_id call_id, SipCall *psipcall, char *Bss_selected_method, CORESIP_CallFlags flags)
{
	if (call_id == PJSUA_INVALID_ID)
	{
		return -1;
	}

	int ret = 0;
	bool found_fr = false;
	bool found_cid = false;
	int i, j;
	int ngroups_aux = ngroups;	
	CORESIP_CallInfo *_Info = psipcall->GetCORESIP_CallInfo();
	char *zona = _Info->Zona;	
	CORESIP_CLD_CALCULATE_METHOD metCld = _Info->CLDCalculateMethod;

	psipcall->_Index_group = INVALID_GROUP_INDEX;
	psipcall->_Index_sess = INVALID_SESS_INDEX;

	pj_mutex_lock(fd_mutex);
	//Se comprueba si ya existe un grupo con esos identificadores de zona y frecuencia que tiene ese call_id y session
	for (i = 0; i < MAX_GROUPS; i++)
	{
		if (STRCMP(groups[i].RdFr, rdfr) == 0 && STRCMP(groups[i].Zona, zona) == 0)
		{
			found_fr = true;
			for (j = 0; j < MAX_SESSIONS; j++)
			{
				if (groups[i].sessions[j].sess_callid == call_id)
				{
					found_cid = true;
					break;
				}
			}
			break;
		}
	}

	if (!found_cid)
	{
		//No se ha encontrado el call_id
		if (!found_fr)
		{
			//No se ha encontrado un grupo con los identificadores de frecuencia y zona.
			//Entonces se crea
			for (i = 0; i < MAX_GROUPS; i++)
			{
				if (strlen(groups[i].RdFr) == 0 || strlen(groups[i].Zona) == 0)
				{	
					if (strlen(rdfr) < sizeof(groups[i].RdFr) && strlen(zona) < sizeof(groups[i].Zona))
					{
						strcpy(groups[i].RdFr, rdfr);
						strcpy(groups[i].Zona, zona);
						groups[i].mcast_seq = 0;
						groups[i]._RdSendTo = NULL;
						groups[i].SelectedUri[0] = '\0';
						groups[i].SelectedUriPttId = 0;
						ngroups_aux++;
					}
					else
					{
						PJ_LOG(3,(__FILE__, "ERROR: Identificador de frecuencia o de grupo demasiado largo \n"));
						ret = -1;   //Cadena de rdfr demasiado larga
					}
					break;
				}
			}
			if (i == MAX_GROUPS) 
			{
				PJ_LOG(3,(__FILE__, "ERROR: Sobrepasado en numero de grupos \n"));
				ret = -1;   //No hay ning�n grupo libre
			}
		}

		//Se a�ade el call_id al grupo
		if (ret == 0)
		{
			for (j = 0; j < MAX_SESSIONS; j++)
			{
				if (groups[i].sessions[j].sess_callid == PJSUA_INVALID_ID)
				{
					groups[i].sessions[j].sess_callid = call_id;
					groups[i].sessions[j].Flags = flags;
					groups[i].sessions[j].pSipcall = psipcall;
					groups[i].sessions[j].TdTxIP = INVALID_TIME_DELAY;
					groups[i].sessions[j].Tj1 = INVALID_TIME_DELAY;
					groups[i].sessions[j].Tj1_count = 0;
					groups[i].sessions[j].Tn1_count = 0;
					groups[i].sessions[j].cld_absoluto = PJ_FALSE;
					groups[i].sessions[j].Bss_value = 0;
					groups[i].sessions[j].Bss_type = 0;
					groups[i].sessions[j].Tred = 0;
					groups[i].sessions[j].squ_status = PJ_FALSE;
					groups[i].sessions[j].selected = PJ_FALSE;
					groups[i].sessions[j]._MetodoClimax = metCld;
					groups[i].sessions[j].Tid_orig = 0;
					groups[i].sessions[j].cld_prev = INVALID_CLD_PREV;		
					groups[i].sessions[j].in_window_timer = PJ_FALSE; 
					if (Bss_selected_method)
					{
						strncpy(groups[i].sessions[j].Bss_selected_method, Bss_selected_method, 
							sizeof(groups[i].sessions[j].Bss_selected_method)-1);
						groups[i].sessions[j].Bss_selected_method[sizeof(groups[i].sessions[j].Bss_selected_method)-1] = '\0';
					}
					groups[i].nsessions++;
					ret = groups[i].nsessions;

					if (flags & CORESIP_CALL_RD_RXONLY) groups[i].nsessions_rx_only++;
					else if (flags & CORESIP_CALL_RD_TXONLY) groups[i].nsessions_tx_only++;

					psipcall->_Index_group = i;
					psipcall->_Index_sess = j;
					break;
				}
			}
			if (j == MAX_SESSIONS) 
			{
				PJ_LOG(3,(__FILE__, "ERROR: Sobrepasado en numero de sesiones en un grupo \n"));
				ret = -1;	//No hay sesiones libres para el grupo
			}
		}
	}
	else
	{
		ret = -1;
		PJ_LOG(3,(__FILE__, "ERROR: El Call_id y session ya esta dado de alta en un grupo \n"));
	}

	if (ret >= 0) 
	{
		ngroups = ngroups_aux;
	}

	pj_mutex_unlock(fd_mutex);

	if (ret >= 0) 
	{
		UpdateGroupClimaxParams(psipcall->_Index_group);
	}
	
	return ret;
}

/**
 * RemFromGroup.	...
 * Borra una sesion de un grupo. Si el grupo se queda vac�o entonces lo elimina. 
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion
 * @return	Numero de sesiones en el grupo. -1 su hay error y no se ha podido quitar del grupo.
 */
int FrecDesp::RemFromGroup(int index_group, int index_sess)
{
	int ret = 0;

	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS)
	{
		//PJ_LOG(3,(__FILE__, "ERROR: FrecDesp::RemFromGroup index_group (%d) o index_sess (%d) son erroneos \n", index_group, index_sess));
		return -1;
	}	

	pj_mutex_lock(fd_mutex);

	if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}

	if ((groups[index_group].sessions[index_sess].Flags & CORESIP_CALL_RD_RXONLY) && (groups[index_group].nsessions_rx_only > 0))
		groups[index_group].nsessions_rx_only--;
	else if ((groups[index_group].sessions[index_sess].Flags & CORESIP_CALL_RD_TXONLY) && (groups[index_group].nsessions_tx_only > 0))
		groups[index_group].nsessions_tx_only--;

	groups[index_group].sessions[index_sess].sess_callid = PJSUA_INVALID_ID;
	groups[index_group].sessions[index_sess].pSipcall = NULL;
	groups[index_group].sessions[index_sess].TdTxIP = INVALID_TIME_DELAY;
	groups[index_group].sessions[index_sess].Tj1 = INVALID_TIME_DELAY;
	groups[index_group].sessions[index_sess].Tj1_count = 0;
	groups[index_group].sessions[index_sess].Tn1_count = 0;
	groups[index_group].sessions[index_sess].cld_absoluto = PJ_FALSE;
	groups[index_group].sessions[index_sess].Bss_value = 0;
	groups[index_group].sessions[index_sess].Bss_type = 0;
	groups[index_group].sessions[index_sess].Tred = 0;
	groups[index_group].sessions[index_sess].squ_status = PJ_FALSE;
	groups[index_group].sessions[index_sess].selected = PJ_FALSE;
	groups[index_group].sessions[index_sess]._MetodoClimax = Relative;
	groups[index_group].sessions[index_sess].Bss_selected_method[0] = '\0';
	groups[index_group].sessions[index_sess].Tid_orig = 0;
	groups[index_group].sessions[index_sess].cld_prev = INVALID_CLD_PREV;
	groups[index_group].sessions[index_sess].in_window_timer = PJ_FALSE;

	if (groups[index_group].nsessions > 0) groups[index_group].nsessions--;
	ret = groups[index_group].nsessions;

	if (groups[index_group].nsessions == 0)
	{
		//El grupo se queda vac�o. Lo eliminamos
		groups[index_group].RdFr[0] = 0;
		groups[index_group].Zona[0] = 0;
		groups[index_group].mcast_seq = 0;
		groups[index_group].nsessions_rx_only = 0;
		groups[index_group].nsessions_tx_only = 0;
		groups[index_group]._RdSendTo = NULL;
		groups[index_group].SelectedUri[0] = '\0';
		groups[index_group].SelectedUriPttId = 0;
		if (ngroups > 0) ngroups--;
		pj_mutex_unlock(fd_mutex);
	}
	else
	{
		pj_mutex_unlock(fd_mutex);
		UpdateGroupClimaxParams(index_group);
	}	

	return ret;
}

/**
 * GetSessionsCountInGroup.	...
 * Retorna el numero de sesiones que tiene el grupo climax al que pertenece un call id. 
 * @param	index_group		Indice del grupo 
 * @param	nsessions_rx_only	Numero de sesiones RX only
 * @param	nsessions_tx_only	Numero de sesiones TX only
 * @return	Numero total de sesiones en el grupo. -1 su hay error 
 */
int FrecDesp::GetSessionsCountInGroup(int index_group, int *nsessions_rx_only, int *nsessions_tx_only)
{
	int ret;

	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		PJ_LOG(3,(__FILE__, "ERROR: FrecDesp::GetSessionsCountInGroup index_group (%d) es erroneo \n", index_group));
		return -1;
	}

	pj_mutex_lock(fd_mutex);
	ret = groups[index_group].nsessions;
	if (nsessions_rx_only != NULL) *nsessions_rx_only = groups[index_group].nsessions_rx_only;
	if (nsessions_tx_only != NULL) *nsessions_tx_only = groups[index_group].nsessions_tx_only;
	pj_mutex_unlock(fd_mutex);

	return ret;
}

/**
 * UpdateGroupClimaxParams.	...
 * Actualiza los par�metros de las sesiones activas en un grupo CLIMAX. 
 * @param	index_group		Indice del grupo 
 * @return	Numero de sesiones en el grupo actualizadas. -1 si hay error, p ej que el grupo no existe.
 */
int FrecDesp::UpdateGroupClimaxParams(int index_group)
{
	int j;
	int ret = 0;

	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return -1;
	}
	
	pj_mutex_lock(fd_mutex);
	for (j = 0; j < MAX_SESSIONS; j++)
	{
		if (groups[index_group].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
		if (!pjsua_call_is_active(groups[index_group].sessions[j].sess_callid)) continue;
		if (!pjsua_call_has_media(groups[index_group].sessions[j].sess_callid)) continue;

		pj_bool_t NTP_sync;			
		if (groups[index_group].sessions[j]._MetodoClimax == Absolute && NTP_synchronized == PJ_TRUE) NTP_sync = PJ_TRUE;
		else NTP_sync = PJ_FALSE;
		pjmedia_session *ses = pjsua_call_get_media_session(groups[index_group].sessions[j].sess_callid);
		if (ses != NULL)
		{
			pjmedia_session_set_climax_param(ses, NTP_sync);
			ret++;
		}
	}
	pj_mutex_unlock(fd_mutex);
	
	return ret;
}

/**
 * UpdateGroupClimaxParamsAllGroups.	...
 * Actualiza los par�metros de las sesiones activas en todos los grupos CLIMAX 
 * @return	-1 si hay error.
 */
int FrecDesp::UpdateGroupClimaxParamsAllGroups()
{
	int i, j;
	int ret = 0;

	pj_mutex_lock(fd_mutex);
	for (i = 0; i < MAX_GROUPS; i++)
	{
		if (strlen(groups[i].RdFr) > 0)
		{
			for (j = 0; j < MAX_SESSIONS; j++)
			{
				if (groups[i].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
				if (!pjsua_call_is_active(groups[i].sessions[j].sess_callid)) continue;
				if (!pjsua_call_has_media(groups[i].sessions[j].sess_callid)) continue;

				pj_bool_t NTP_sync;			
				if (groups[i].sessions[j]._MetodoClimax == Absolute && NTP_synchronized == PJ_TRUE) NTP_sync = PJ_TRUE;
				else NTP_sync = PJ_FALSE;
				pjmedia_session *ses = pjsua_call_get_media_session(groups[i].sessions[j].sess_callid);
				if (ses != NULL)
				{
					pjmedia_session_set_climax_param(ses, NTP_sync);
				}
			}
		}		
	}
	pj_mutex_unlock(fd_mutex);

	return ret;
}

/**
 * SetTimeDelay.	...
 * Asigna el time delay calculado a partir del MAM recibido a la sesion correspondiente dentro de un grupo. 
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion
 * @param   ext_value		Puntero donde comienza el value del TLV
 * @param   request_MAM     Retorna si el bit NMR est� a 1 y por tanto hay que enviar un RMM en el siguiente paquete RTP
 * @return	Numero de time delays asignadas a la sesion. -1 su hay error.
 */
int FrecDesp::SetTimeDelay(pjmedia_stream *stream, CORESIP_PttType ptttype, int index_group, int index_sess, pj_uint8_t *ext_value, pj_bool_t *request_MAM)
{
	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		PJ_LOG(3,(__FILE__, "ERROR: FrecDesp::SetTimeDelay index_group (%d) o index_sess (%d) son erroneos \n", index_group, index_sess));
		return -1;
	}

	pj_uint32_t TQG, T1, NMR, T2, Tsd, Tj1, Tid, T4;

	TQG = (pj_uint32_t) *ext_value;
	TQG >>= 7;
	TQG &= 0x1;

	T1 = (pj_uint32_t) *ext_value;
	T1 &= 0x7F;
	T1 <<= 8;
	ext_value++;
	T1 |= (pj_uint32_t) *ext_value;
	ext_value++;
	T1 <<= 8;
	T1 |= (pj_uint32_t) *ext_value;

	pj_uint32_t last_T1 = 0;
	pjmedia_stream_get_last_T1(stream, &last_T1);
	if (T1 != last_T1 || last_T1 == 0)
	{
		//El T1 recibido no es igual al enviado en el RMM. Por tanto no lo tenemos en cuenta
		PJ_LOG(3,(__FILE__, "ERROR: CLIMAX: T1 recibido en MAM (0x%08X) no corresponde con el enviado en RMM (0x%08X)", T1, last_T1));
		return -1;
	}

	ext_value++;
	NMR = (pj_uint32_t) ((*ext_value >> 7) & 0x01);
	T2 = (pj_uint32_t) (*ext_value & 0x7F);
	ext_value++;
	T2 <<= 8;
	T2 |= (pj_uint32_t) *ext_value;
	ext_value++;
	T2 <<= 8;
	T2 |= (pj_uint32_t) *ext_value;
		
	ext_value++;
	Tsd = (pj_uint32_t) *ext_value;
	ext_value++;
	Tsd <<= 8;
	Tsd |= (pj_uint32_t) *ext_value;

	ext_value++;
	Tj1 = (pj_uint32_t) *ext_value;
	ext_value++;
	Tj1 <<= 8;
	Tj1 |= (pj_uint32_t) *ext_value;

	ext_value++;
	Tid = (pj_uint32_t) *ext_value;
	ext_value++;
	Tid <<= 8;
	Tid |= (pj_uint32_t) *ext_value;	

	FILETIME SystemTimeAsFileTime;
	unsigned long long ullT4, ullT4_seg;			

	GetSystemTimeAsFileTime(&SystemTimeAsFileTime);  	
		
	//ullT4 en unidades de 100ns. En formato FILETIME. Desde 0 horas 1/1/1601
	ullT4 = (unsigned long long) SystemTimeAsFileTime.dwHighDateTime;
	ullT4 <<= 32;
	ullT4 |= (unsigned long long) SystemTimeAsFileTime.dwLowDateTime;

	//Se le resta el tiempo en unidades de 100ns desde 0 horas 1/1/1601 a 0 horas 1/1/1900
	//Para convertirlo en NTP timestamp
	ullT4 -= (unsigned long long) 94354848000000000;

	ullT4_seg = ullT4 / 10000000;   //Timestamp en segundos. Ser�an los 32bits de mayor peso de un NTP timestamp de 64 bits
	ullT4 -= ullT4_seg * 10000000;  //A ullT4 le restamos la cantidad de segundos pero en unidades de 100ns
									//obteniedo la fraccion de segundos en unidades de 100ns			
	ullT4_seg &= 0x3FF;				//Nos quedamos con los 10 de menor peso, que sumados a 32 nos quedar�amos con 42
	ullT4 = ullT4_seg * 10000000 + ullT4; //Le volvermos a sumar los segundos ya truncados.
	ullT4 /= 1250;					//ullT4 En unidades de 125 us
	ullT4 &= 0x7FFFFF;				//Nos quedamos con 23 bits
		
	T4 = (pj_uint32_t) ullT4;	
		
	pj_uint32_t TdTxIP = 0;
	pj_uint32_t Tn1 = 0;

	//Calculamos el Time delay en Tx.   
	CORESIP_CLD_CALCULATE_METHOD NTP_sync = groups[index_group].sessions[index_group]._MetodoClimax;

	pj_bool_t metodo_absoluto = PJ_FALSE;
	if (NTP_sync == Absolute && TQG != 0)
	{
		if (T2 >= T1)
		{
			//Metodo absoluto
			// TdTxIP = Tv1+Tp1+T2�T1+Tj1+Tid
			if (T2-T1 > (ui32_OFFSET_THRESHOLD_us/125)) 
				Tn1 = T2 - T1;
			else 
				Tn1 = 0;				
			metodo_absoluto = PJ_TRUE;
		}
		else 
		{
			if (T1-T2 > (ui32_OFFSET_THRESHOLD_us/125)) 
			{
				//Aunque se est� sincronizado, si T2 < T1, entonces no podemos considerar que est�n sincronizado
				//Puede ser porque haya un offset negativo. Se usa el metodo relativo
				metodo_absoluto = PJ_FALSE;
			}
			else 
			{
				//Si la diferencia es peque�a entonces consideramos un retardo de red de cero
				Tn1 = 0;
				metodo_absoluto = PJ_TRUE;
			}
		}
	}


	if (!metodo_absoluto)
	{   // TdTxIP = Tv1+Tp1+Tn1+Tj1+Tid. Tn1 =  (T4 - T1 - Tsd) / 2
		if (T4 < T1) Tn1 = 0;
		else Tn1 = T4 - T1;
		if (Tn1 >= Tsd) Tn1 -= Tsd;
		else Tn1 = 0;
		Tn1 /= 2; 
	}	

	/*
	if (NMR != 0)
	{
		*request_MAM = PJ_TRUE;
		return -1;
	}
	*/

	PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s PTT %d TQG 0x%X T1 0x%X NMR 0x%X T2 0x%X Tsd 0x%X Tj1 0x%X Tid 0x%X, T4 0x%X, T4-T1 %d us, Tred %d us, Absoluto %d", groups[index_group].RdFr, ptttype, TQG, T1, NMR, T2, Tsd, Tj1, Tid, T4, (T4-T1)*125, Tn1*125, metodo_absoluto));

	pj_mutex_lock(fd_mutex);

	if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}
	if (!pjsua_call_is_active(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}
	if (!pjsua_call_has_media(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}

/*
	if ((NMR == 0) && (groups[index_group].sessions[index_sess].TdTxIP != INVALID_TIME_DELAY))
	{
		//Si NMR es 0 no recalculamos
		pj_mutex_unlock(fd_mutex);
		return 0;
	}
*/


	/*if (groups[index_group].sessions[index_sess].Tid_orig != Tid)
	{
		PJ_LOG(5,(__FILE__, "CLIMAX: Tid DISTINTO Anterior 0x%X Actual 0x%X PTT %d", groups[index_group].sessions[index_sess].Tid_orig, Tid, ptttype));
	}*/

	if (groups[index_group].sessions[index_sess].TdTxIP == INVALID_TIME_DELAY)
	{
		//Es el primer MAM recibido despu�s de que ha sido establecida la sesion
		//Salvamos el Tid que nos dan
		groups[index_group].sessions[index_sess].Tid_orig = Tid;
	}
	else
	{
		Tid = groups[index_group].sessions[index_sess].Tid_orig;
	}

	//Filtramos el Tj1
	groups[index_group].sessions[index_sess].Tj1 = Tj1;
/*
	if (groups[index_group].sessions[index_sess].Tj1 == INVALID_TIME_DELAY)
	{
		groups[index_group].sessions[index_sess].Tj1 = Tj1;
		groups[index_group].sessions[index_sess].Tj1_count = 0;
	}
	else if (groups[index_group].sessions[index_sess].Tj1 > Tj1)
	{
		if ((groups[index_group].sessions[index_sess].Tj1 - Tj1) > 96)  //12 ms
		{
			if (++groups[index_group].sessions[index_sess].Tj1_count > Tj1_count_MAX)
			{
				PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s Tj1 DISTINTO MAS DE %d VECES  !!!!!!!!!", groups[index_group].RdFr, Tj1_count_MAX));
				groups[index_group].sessions[index_sess].Tj1 = Tj1;
				groups[index_group].sessions[index_sess].Tj1_count = 0;
			}			
			else
			{
				Tj1 = groups[index_group].sessions[index_sess].Tj1;
			}
		}
		else
		{
			groups[index_group].sessions[index_sess].Tj1 = Tj1;
			groups[index_group].sessions[index_sess].Tj1_count = 0;
		}
	}
	else if (groups[index_group].sessions[index_sess].Tj1 < Tj1)
	{
		if ((Tj1 - groups[index_group].sessions[index_sess].Tj1) > 96)	//12 ms
		{
			if (++groups[index_group].sessions[index_sess].Tj1_count > Tj1_count_MAX)
			{
				PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s Tj1 DISTINTO MAS DE %d VECES  !!!!!!!!!", groups[index_group].RdFr, Tj1_count_MAX));
				groups[index_group].sessions[index_sess].Tj1 = Tj1;
				groups[index_group].sessions[index_sess].Tj1_count = 0;
			}
			else
			{
				Tj1 = groups[index_group].sessions[index_sess].Tj1;
			}
		}
		else
		{
			groups[index_group].sessions[index_sess].Tj1 = Tj1;
			groups[index_group].sessions[index_sess].Tj1_count = 0;
		}
	}
	else
	{
		groups[index_group].sessions[index_sess].Tj1_count = 0;
		groups[index_group].sessions[index_sess].Tj1 = Tj1;
	}
*/


	//Filtramos el Tred	
	/*
	if (groups[index_group].sessions[index_sess].TdTxIP == INVALID_TIME_DELAY)
	{
		groups[index_group].sessions[index_sess].Tred = Tn1;
		groups[index_group].sessions[index_sess].Tn1_count = 0;
	}
	else if (Tn1 >= groups[index_group].sessions[index_sess].Tred)
	{
		if ((Tn1 - groups[index_group].sessions[index_sess].Tred) > 160)  //20 ms
		{
			PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s Tn1-Tn1prev %d us", groups[index_group].RdFr, (Tn1-groups[index_group].sessions[index_sess].Tred)*125));
			PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s SE PASA  !!!!!!!!!!!!", groups[index_group].RdFr));
			if (++groups[index_group].sessions[index_sess].Tn1_count > Tn1_count_MAX)
			{
				PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s Tn1 DISTINTO MAS DE %d VECES  !!!!!!!!!", groups[index_group].RdFr, Tn1_count_MAX));
				groups[index_group].sessions[index_sess].Tred = Tn1;
				groups[index_group].sessions[index_sess].Tn1_count = 0;
			}
			else
			{
				Tn1 = groups[index_group].sessions[index_sess].Tred;
			}
		}
		else
		{
			groups[index_group].sessions[index_sess].Tred = Tn1;
			groups[index_group].sessions[index_sess].Tn1_count = 0;
		}
	}
	else if (Tn1 < groups[index_group].sessions[index_sess].Tred)
	{
		if ((groups[index_group].sessions[index_sess].Tred - Tn1) > 160)  //20 ms
		{
			PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s Tn1prev-Tn1 %d us", groups[index_group].RdFr, (groups[index_group].sessions[index_sess].Tred-Tn1)*125));
			PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s SE PASA  !!!!!!!!!!!!", groups[index_group].RdFr));
			if (++groups[index_group].sessions[index_sess].Tn1_count > Tn1_count_MAX)
			{
				PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s Tn1 DISTINTO MAS DE %d VECES  !!!!!!!!!", groups[index_group].RdFr, Tn1_count_MAX));
				groups[index_group].sessions[index_sess].Tred = Tn1;
				groups[index_group].sessions[index_sess].Tn1_count = 0;
			}
			else
			{
				Tn1 = groups[index_group].sessions[index_sess].Tred;
			}
		}
		else
		{
			groups[index_group].sessions[index_sess].Tred = Tn1;
			groups[index_group].sessions[index_sess].Tn1_count = 0;
		}
	}	
	*/

	groups[index_group].sessions[index_sess].Tred = Tn1;

	PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay 2 Fr %s TQG 0x%X T1 0x%X NMR 0x%X T2 0x%X Tsd 0x%X Tj1 0x%X Tid 0x%X, T4 0x%X, T4-T1 %d us, Tred %d us, Absoluto %d", groups[index_group].RdFr, TQG, T1, NMR, T2, Tsd, Tj1, Tid, T4, (T4-T1)*125, Tn1*125, metodo_absoluto));
	//Se calcula el retardo
	TdTxIP = Tv1 + Tp1 + Tn1 + Tj1 + Tid;
	groups[index_group].sessions[index_sess].TdTxIP = TdTxIP;
	groups[index_group].sessions[index_sess].cld_absoluto = metodo_absoluto;
	
	pj_mutex_unlock(fd_mutex);

	PJ_LOG(5,(__FILE__, "CLIMAX: SetTimeDelay Fr %s TdTxIP %d us", groups[index_group].RdFr, TdTxIP*125));
	
	return 1;	
}

/**
 * GetRetToApply.	...
 * Calcula el el retardo a aplicar. Es la diferencia entre el maximo retardo de red y el de la sesion actual
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion 
 * @return  El retardo en unidades de 125us. 
 */
pj_uint32_t FrecDesp::GetRetToApply(int index_group, int index_sess)  
{
	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return 0;
	}

	pj_mutex_lock(fd_mutex);

	if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
	{
		pj_mutex_unlock(fd_mutex);
		return 0;
	}
	if (!pjsua_call_is_active(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return 0;
	}	
	if (!pjsua_call_has_media(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return 0;
	}

	pj_uint32_t ret = 0;

	pj_uint32_t Tred_max = 0;
	for (int j = 0; j < MAX_SESSIONS; j++)
	{
		if (groups[index_group].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
		if (!pjsua_call_is_active(groups[index_group].sessions[j].sess_callid)) continue;
		if (!pjsua_call_has_media(groups[index_group].sessions[j].sess_callid)) continue;

		if (groups[index_group].sessions[j].pSipcall != NULL)
		{
			CORESIP_CallInfo *_Info = groups[index_group].sessions[j].pSipcall->GetCORESIP_CallInfo();
			if (!(_Info->Flags & CORESIP_CALL_RD_TXONLY))
			{
				if (groups[index_group].sessions[j].TdTxIP != INVALID_TIME_DELAY)
				{
					if (groups[index_group].sessions[j].Tred > Tred_max)
					{
						Tred_max = groups[index_group].sessions[j].Tred;
					}
				}				
			}
		}
	}	

	ret = Tred_max - groups[index_group].sessions[index_sess].Tred;

	pj_mutex_unlock(fd_mutex);

	return ret;
}

/**
 * GetQidx.	...
 * Toma indice de calidad y metodo para una sesion dentro de un grupo. 
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion 
 * @param	qidx		Indice de calidad
 * @param	qidx_ml		M�todo del indice de calidad
 * @param   Tred		Retorna retardo de red de esta sesi�n. En unidades de 125 us
 * @return	
 */
void FrecDesp::GetQidx(int index_group, int index_sess, pj_uint8_t *qidx, pj_uint8_t *qidx_ml, pj_uint32_t *Tred)
{
	*Tred = 0;
	*qidx = 0;
	*qidx_ml = 0;

	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		PJ_LOG(5,(__FILE__, "ERROR: FrecDesp::GetQidx index_group (%d) o index_sess (%d) son erroneos \n", index_group, index_sess));
		return;
	}

	pj_mutex_lock(fd_mutex);

	if ((strlen(groups[index_group].RdFr) == 0) ||
			(strlen(groups[index_group].Zona) == 0))
	{
		pj_mutex_unlock(fd_mutex);
		return;
	}
	if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
	{
		pj_mutex_unlock(fd_mutex);
		return;
	}
	if (!pjsua_call_is_active(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return;
	}
	if (!pjsua_call_has_media(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return;
	}

	*qidx = groups[index_group].sessions[index_sess].Bss_value;
	*qidx_ml = groups[index_group].sessions[index_sess].Bss_type;
	*Tred = groups[index_group].sessions[index_sess].Tred;

	pj_mutex_unlock(fd_mutex);
}

/**
 * GetLastCld.	...
 * Retorna el ultimo cld calculado. 
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion 
 * @param	cld		cld que retorna. SIN INDICADOR DE METODO ABSOLUTO
 * @param	qidx_ml		M�todo del indice de calidad
 * @param   Tred		Retorna retardo de red de esta sesi�n. En unidades de 125 us
 * @return	-1 si error
 */
int FrecDesp::GetLastCld(int index_group, int index_sess, pj_uint8_t *cld)
{	
	int ret = 0;
	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS || cld == NULL) 
	{
		return -1;
	}

	*cld = 0;
	pj_mutex_lock(fd_mutex);

	if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}
	if (!pjsua_call_is_active(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}
	if (!pjsua_call_has_media(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}

	pj_uint32_t cld_prev = groups[index_group].sessions[index_sess].cld_prev;
	if (cld_prev == INVALID_CLD_PREV)
	{
		ret = -1;
	}
	else
	{
		ret = 0;
		*cld = (pj_uint8_t) cld_prev;
	}
	pj_mutex_unlock(fd_mutex);
	return ret;
}

/**
 * GetCLD.	...
 * Calcula el CLD. 
 * @param	call_id		Call id 
 * @param	cld		Valor del cld que se retorna 
 * @return	-1 si hay error.
 */
int FrecDesp::GetCLD(pjsua_call_id call_id, pj_uint8_t *cld)
{
	int i, j;
	int group_index = -1;					//Indice del array groups donde se encuentra el call_id
	int sess_index = -1;					//Indice del array de sesiones dentro del grupo donde se encuentra el call_id
	pj_uint32_t max_delay_in_group = 0;		//Maximo retardo en el grupo
	pj_uint32_t delay_diff = 0;				//Diferencia entre el maximo retardo en el grupo y el correspondiente a este call_id
	int valid_sesion_cnt = 0;

	if (cld == NULL) return -1;
	*cld = 0;
	if (call_id == PJSUA_INVALID_ID) return -1;

	//Se busca el grupo en el que est� el call_id
	pj_mutex_lock(fd_mutex);
	for (i = 0; i < MAX_GROUPS; i++)
	{
		if (strlen(groups[i].RdFr) > 0 && strlen(groups[i].Zona) > 0)
		{
			max_delay_in_group = 0;
			valid_sesion_cnt = 0;
			for (j = 0; j < MAX_SESSIONS; j++)
			{
				if (groups[i].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
				if (!pjsua_call_is_active(groups[i].sessions[j].sess_callid)) continue;
				if (!pjsua_call_has_media(groups[i].sessions[j].sess_callid)) continue;

				if (!(groups[i].sessions[j].Flags & CORESIP_CALL_RD_RXONLY))
				{
					if (groups[i].sessions[j].TdTxIP != INVALID_TIME_DELAY)
					{
						if (groups[i].sessions[j].TdTxIP > max_delay_in_group)
						{
							max_delay_in_group = groups[i].sessions[j].TdTxIP;						
						}
						valid_sesion_cnt++;
					}

					if (groups[i].sessions[j].sess_callid == call_id)
					{
						group_index = i;
						sess_index = j;
					}					
				}
			}
			if (group_index != -1) break;  //Se ha encontrado el grupo donde est� el call_id
		}
	}

	if (group_index == -1 || sess_index == -1) 
	{
		pj_mutex_unlock(fd_mutex);
		return -1;					
	}

	if (groups[group_index].sessions[sess_index].TdTxIP == INVALID_TIME_DELAY)
	{		
		//Todav�a no ha habido c�lculo del retardo retornamos un cld=0 y con error.
		pj_mutex_unlock(fd_mutex);
		return -1;
	}

	delay_diff = max_delay_in_group - groups[group_index].sessions[sess_index].TdTxIP;
	//Se pasa a unidades de  2 ms. delay_diff * 125 / 1000 / 2 = delay_diff / 4
	delay_diff /= 16;	//Unidades de 2ms

	//delay_diff += 1;	//Por error de jotron. No se le puede mandar un cld a cero
		
	if ((groups[group_index].sessions[sess_index].cld_prev == 0) && (delay_diff <= 1))
	{
		//Solo se envía un cld a valor 0
		pj_mutex_unlock(fd_mutex);
		return -1;
	}
	else
	{
		groups[group_index].sessions[sess_index].cld_prev = (delay_diff & 0x7F);
	}		

	//groups[group_index].sessions[sess_index].cld_prev = (delay_diff & 0x7F);

	if (groups[group_index].sessions[sess_index].cld_absoluto)
	{
		*cld |= 0x80;	
	}

	*cld |= (pj_uint8_t) delay_diff;

	pj_mutex_unlock(fd_mutex);

	return 0;
}

/**
 * SetSquSt.	...
 * Actualiza el estado del squelch para una sesion en el grupo y retorna el numero de squelch activados en ese grupo. 
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion
 * @param   squ_st.			Estado del squelch a actualizar
 * @param	p_sq_air_count	Retorna contador de squelches de avion activados en el grupo 
 * @return	Cantidad de squelch activados en el grupo. Retorna -1 si no existe el grupo. 
 */
int FrecDesp::SetSquSt(int index_group, int index_sess, pj_bool_t squ_st, int *p_sq_air_count)
{
	int j;
	pj_bool_t sess_found = PJ_FALSE;
	int squ_count = -1;

	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		PJ_LOG(3,(__FILE__, "ERROR: FrecDesp::SetSquSt index_group (%d) o index_sess (%d) son erroneos \n", index_group, index_sess));
		return -1;
	}

	int sq_air_count = 0;

	pj_mutex_lock(fd_mutex);	
	
	if (strlen(groups[index_group].RdFr) > 0 && strlen(groups[index_group].Zona) > 0)
	{
		if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
		{
			pj_mutex_unlock(fd_mutex);
			return -1;
		}
		if (!pjsua_call_is_active(groups[index_group].sessions[index_sess].sess_callid))
		{
			pj_mutex_unlock(fd_mutex);
			return -1;
		}
		if (!pjsua_call_has_media(groups[index_group].sessions[index_sess].sess_callid))
		{
			pj_mutex_unlock(fd_mutex);
			return -1;
		}

		groups[index_group].sessions[index_sess].squ_status = squ_st;
		squ_count = 0;

		for (j = 0; (j < MAX_SESSIONS); j++)
		{				
			if (groups[index_group].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
			if (!pjsua_call_is_active(groups[index_group].sessions[j].sess_callid)) continue;
			if (!pjsua_call_has_media(groups[index_group].sessions[j].sess_callid)) continue;

			if (groups[index_group].sessions[j].squ_status) 
			{
				squ_count++;		
				if (groups[index_group].sessions[j].pSipcall->RdInfo_prev.PttId == 0)
				{
					sq_air_count++;
				}
			}	
		}
	}

	pj_mutex_unlock(fd_mutex);

	if (p_sq_air_count != NULL) *p_sq_air_count = sq_air_count;

	return squ_count;
}

/**
 * ClrAllSquSt.	...
 * Desactiva todos los estados de squelch de las sesiones de un grupo. También en el objeto SipCall
 * @param	index_group		Indice del grupo
 * @return	-1 si hay un error.
 */
int FrecDesp::ClrAllSquSt(int index_group)
{
	int j;
	int ret = 0;

	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		PJ_LOG(3,(__FILE__, "ERROR: FrecDesp::ClrAllSquSt index_group (%d) es erroneo", index_group));
		return -1;
	}
	
	pj_mutex_lock(fd_mutex);	
	
	if (strlen(groups[index_group].RdFr) > 0 && strlen(groups[index_group].Zona) > 0)
	{
		for (j = 0; (j < MAX_SESSIONS); j++)
		{				
			if (groups[index_group].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
			if (!pjsua_call_is_active(groups[index_group].sessions[j].sess_callid)) continue;
			if (!pjsua_call_has_media(groups[index_group].sessions[j].sess_callid)) continue;

			groups[index_group].sessions[j].squ_status = PJ_FALSE;
			if (groups[index_group].sessions[j].pSipcall != NULL)
			{
				groups[index_group].sessions[j].pSipcall->squ_status = PJ_FALSE;
			}				
		}
	}
	else
	{
		ret = -1;
	}

	pj_mutex_unlock(fd_mutex);

	return ret;
}

/**
 * GetSquCnt.	...
 * Retorna el numero de squelch activados en ese grupo. 
 * @param	index_group		Indice del grupo
 * @return	Cantidad de squelch activados en el grupo. Retorna -1 si no existe el grupo. 
 */
int FrecDesp::GetSquCnt(int index_group)
{
	int j;
	pj_bool_t sess_found = PJ_FALSE;
	int squ_count = 0;

	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return squ_count;
	}
	
	pj_mutex_lock(fd_mutex);		

	if (strlen(groups[index_group].RdFr) > 0 && strlen(groups[index_group].Zona) > 0)
	{		
		for (j = 0; (j < MAX_SESSIONS); j++)
		{	
			if (groups[index_group].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
			if (!pjsua_call_is_active(groups[index_group].sessions[j].sess_callid)) continue;
			if (!pjsua_call_has_media(groups[index_group].sessions[j].sess_callid)) continue;

			if (groups[index_group].sessions[j].squ_status) 
			{
				squ_count++;						
			}					
		}
	}

	pj_mutex_unlock(fd_mutex);

	return squ_count;
}

/**
 * SetInWindow.	...
 * Activa en todas las sesiones del grupo el flag que indica el estado de la ventana de seleccion bss 
 * @param	index_group		Indice del grupo
 * @param   status			Estado de la ventana de selecciona. Activada o no
 * @return	Retorna -1 si no existe el grupo. 
 */
int FrecDesp::SetInWindow(int index_group, pj_bool_t status)
{
	int ret = 0;

	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return -1;
	}
	
	pj_mutex_lock(fd_mutex);	
	
	if (strlen(groups[index_group].RdFr) > 0 && strlen(groups[index_group].Zona) > 0)
	{
		for (int j = 0; (j < MAX_SESSIONS); j++)
		{				
			if (groups[index_group].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
			if (!pjsua_call_is_active(groups[index_group].sessions[j].sess_callid)) continue;
			if (!pjsua_call_has_media(groups[index_group].sessions[j].sess_callid)) continue;

			groups[index_group].sessions[j].in_window_timer = status;					
		}
	}
	else
	{
		ret = -1;
	}

	pj_mutex_unlock(fd_mutex);

	return ret;
}

/**
 * GetInWindow.	...
 * Activa en todas las sesiones del grupo el flag que indica el estado de la ventana de seleccion bss 
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion
 * @return	Retorna si esta en la ventana de seleccion 
 */
pj_bool_t FrecDesp::GetInWindow(int index_group, int index_sess)
{
	int ret = PJ_FALSE;

	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return ret;
	}
	
	pj_mutex_lock(fd_mutex);	
	
	if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}	

	ret = groups[index_group].sessions[index_sess].in_window_timer;

	pj_mutex_unlock(fd_mutex);

	return ret;
}

/**
 * GetNextLineField.	...
 * Devuelve el siguiente string de una linea. Los string est�n separados por espacios 
 * @param	pos. Posicion de la linea a partir de la cual se busca el string
 * @param   line. Buffer de la linea.
 * @param   out. Buffer donde se devuelve el string
 * @return	La posici�n del siguiente caracter despues del string encontrado. 
		-1. si la linea ha finalizado.
 */
int FrecDesp::GetNextLineField(int pos, char *line, char *out, int out_size)
{
	int i, j;

	out[0] = '\0';
	i = pos;
	while (line[i] == ' ') i++;
	if (line[i] == '\0') return -1;
	j = 0;
	while (line[i] != ' ' && line[i] != '\0' && j < (out_size-1))
	{
		out[j] = line[i];
		i++;
		j++;
	}
	out[j] = '\0';
	if (line[i] == '\0') return -1;
	return i;
}

/**
 * SetBss.	...
 * Asigna el valor del BSS (El Qidx) a la sesion correspondiente a partir de la extensi�n de cabecera, del RTP, tambi�n asigna el tipo del bss.
 * Guarda el ultimo recibido para enviarlo a la aplicacion en la callback RdInfoChanged
 * Aparte de a�adirlo a la sesi�n del grupo, retorna los valores extraidos de la extensi�n de cabecera.
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion
 * @param   ext_value		Puntero donde comienza el value del TLV
 * @param   BssMethod       Metodo de bss que retorna. 
 * @param   BssValue		Valor de bss
 * @return	-1 su hay error.
 */
int FrecDesp::SetBss(int index_group, int index_sess, pj_uint8_t *ext_value, int *BssMethod, int *BssValue)
{
	pj_uint8_t bss_type, bss_value;

	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return -1;
	}

	bss_type = *ext_value;
	bss_type &= 0x7;

	bss_value = *ext_value;
	bss_value >>= 3;
	bss_value &= 0x1F;
	
	int ret = 0;
	pj_mutex_lock(fd_mutex);
	if (strlen(groups[index_group].RdFr) > 0 && strlen(groups[index_group].Zona) > 0)
	{				
		if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
		{
			pj_mutex_unlock(fd_mutex);
			return -1;
		}
		if (!pjsua_call_is_active(groups[index_group].sessions[index_sess].sess_callid))
		{
			pj_mutex_unlock(fd_mutex);
			return -1;
		}
		if (!pjsua_call_has_media(groups[index_group].sessions[index_sess].sess_callid))
		{
			pj_mutex_unlock(fd_mutex);
			return -1;
		}

		groups[index_group].sessions[index_sess].Bss_type = bss_type;
		groups[index_group].sessions[index_sess].Bss_value = bss_value;
		*BssMethod = (int) bss_type;
		*BssValue = (int) bss_value;
		ret = 1;
	}
	pj_mutex_unlock(fd_mutex);

	return ret;
}

/**
 * SetBss.	...
 * Asigna el valor del BSS (El Qidx) a la sesion correspondiente.
 * Guarda el ultimo recibido para enviarlo a la aplicacion en la callback RdInfoChanged
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion
 * @param   qidx_method		Metodo de calculo
 * @param   qidx_value		Valor del qidx
 * @return	-1 su hay error.
 */
int FrecDesp::SetBss(int index_group, int index_sess, pj_uint8_t qidx_method, pj_uint8_t qidx_value)
{
	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return -1;
	}

	int ret = 0;
	pj_mutex_lock(fd_mutex);
	if (strlen(groups[index_group].RdFr) > 0 && strlen(groups[index_group].Zona) > 0)
	{				
		if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
		{
			pj_mutex_unlock(fd_mutex);
			return -1;
		}
		if (!pjsua_call_is_active(groups[index_group].sessions[index_sess].sess_callid))
		{
			pj_mutex_unlock(fd_mutex);
			return -1;
		}
		if (!pjsua_call_has_media(groups[index_group].sessions[index_sess].sess_callid))
		{
			pj_mutex_unlock(fd_mutex);
			return -1;
		}

		groups[index_group].sessions[index_sess].Bss_type = qidx_method;
		groups[index_group].sessions[index_sess].Bss_value = qidx_value;
	}
	pj_mutex_unlock(fd_mutex);

	return ret;
}


/**
 * GetBss.	...
 * Retorna el ultimo valor recibido de BSS para una sesion.
 * @param	index_group		Indice del grupo
 * @param   index_sess		Indice de la sesion
 * @param   BssMethod       Metodo de bss que retorna. 
 * @param   BssValue		Valor de bss
 * @return	-1 su hay error.
 */
int FrecDesp::GetBss(int index_group, int index_sess, int *BssMethod, int *BssValue)
{
	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return -1;
	}

	pj_mutex_lock(fd_mutex);
	if (groups[index_group].sessions[index_sess].sess_callid == PJSUA_INVALID_ID)
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}
	if (!pjsua_call_is_active(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}
	if (!pjsua_call_has_media(groups[index_group].sessions[index_sess].sess_callid))
	{
		pj_mutex_unlock(fd_mutex);
		return -1;
	}

	*BssMethod = groups[index_group].sessions[index_sess].Bss_type;
	*BssValue = groups[index_group].sessions[index_sess].Bss_value;

	pj_mutex_unlock(fd_mutex);
	
	return 0;
}

/**
 * Set_group_multicast_socket.	...
 * Se pasan los sockets por los que se enviar� al multicast para este grupo
 * @param	index_group		Indice del grupo
 * @param   RdSendTo		socket
 * @return	-1 su hay error.
 */
int FrecDesp::Set_group_multicast_socket(int index_group, pj_sockaddr_in *RdSendTo)
{
	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return -1;
	}

	pj_mutex_lock(fd_mutex);
	groups[index_group]._RdSendTo = RdSendTo;
	pj_mutex_unlock(fd_mutex);
	return 0;
}

/**
 * Get_group_multicast_socket.	...
 * Se pasan los sockets por los que se enviar� al multicast para este grupo
 * @param	index_group		Indice del grupo
 * @param   RdSendTo		socket que retorna
 * @return	socket.
 */
int FrecDesp::Get_group_multicast_socket(int index_group, pj_sockaddr_in **RdSendTo)
{
	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		*RdSendTo = NULL;
		return -1;
	}

	pj_mutex_lock(fd_mutex);
	*RdSendTo = groups[index_group]._RdSendTo;
	pj_mutex_unlock(fd_mutex);
	return 0;
}

/**
 * Set_mcast_seq.	...
 * 
 * @param	index_group		Indice del grupo
 * @param   mcast_seq		Numero de secuancia a asignar
 * @return	-1 su hay error.
 */
int FrecDesp::Set_mcast_seq(int index_group, unsigned mcast_seq)
{
	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return -1;
	}

	pj_mutex_lock(fd_mutex);
	groups[index_group].mcast_seq = mcast_seq;
	pj_mutex_unlock(fd_mutex);
	return 0;
}

/**
 * Get_mcast_seq.	...
 * 
 * @param	index_group		Indice del grupo
 * @return	mcast_seq.
 */
unsigned FrecDesp::Get_mcast_seq(int index_group)
{
	unsigned ret;

	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return 0;
	}
	
	pj_mutex_lock(fd_mutex);
	ret = groups[index_group].mcast_seq;
	groups[index_group].mcast_seq++;
	pj_mutex_unlock(fd_mutex);


	return ret;
}

/**
 * SetBetterSession.	...
 * Activa la sesion del grupo con mejor BSS. Esa sera la que envie audio por el multicast a los puestos.
 * @param	p_current_sipcall	Puntero al objeto que maneja la sesion actual. 
 * @param   in_window. Indica que la seleccion se hace en la ventana de decisión. 
 * @param   only_selected_in_window. Indica que solamente la mejor en la ventana de decision puede ser seleccionada
 * @return	-1 si hay error.
 */
int FrecDesp::SetBetterSession(SipCall *p_current_sipcall, pj_bool_t in_window, pj_bool_t only_selected_in_window)
{
	int j;

	if (p_current_sipcall == NULL) 
	{
		return -1;
	}

	int index_group = p_current_sipcall->_Index_group;
	int index_sess = p_current_sipcall->_Index_sess;

	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return -1;
	}
	
	pj_mutex_lock(fd_mutex);

	//Tomamos qidx de la sesion actual	
	int current_Bss_value = p_current_sipcall->GetSyncBss();		

	//Literal del metodo seleccionado del sdp del 200ok
	char *curBssSelMethod = groups[index_group].sessions[index_sess].Bss_selected_method;

	//Guardamos el estado actual de squelch
	pj_bool_t current_squ_status = groups[index_group].sessions[index_sess].squ_status;
	
	//Si el squelch no esta activado forzamos un bss negativo.
	if (!current_squ_status) current_Bss_value = -1;	

	if (SipAgent::Coresip_Local_Config._Debug_BSS)
	{
		PJ_LOG(3,(__FILE__, "BSS: SetBetterSession current session  FR %s %s qidx %d", groups[index_group].RdFr, 
			groups[index_group].sessions[index_sess].pSipcall->DstUri, current_Bss_value));
	}

	//Cuenta las sesiones seleccionadas en el grupo
	int sessions_with_multicast_enabled_count = 0;		

	int better_index_sess = -1;
	int better_bss = -1;
	//Se busca en el resto de sesiones del grupo la que tenga un mejor bss 
	for (j = 0; j < MAX_SESSIONS; j++)
	{						
		if (groups[index_group].sessions[j].sess_callid != PJSUA_INVALID_ID &&
			groups[index_group].sessions[j].squ_status == PJ_TRUE &&
			groups[index_group].sessions[j].pSipcall != NULL)
		{
			CORESIP_CallFlags call_flags = groups[index_group].sessions[j].Flags;

			if (!(call_flags & CORESIP_CALL_RD_TXONLY))
			{
				if (j != index_sess)
				{
					int bss_sync = groups[index_group].sessions[j].pSipcall->GetSyncBss();										

					if (strlen(curBssSelMethod) == 0)
					{
						//Si la longitud del string del metodo seleccionado de la actual sesion es cero
						//puede ser porque esa sesion ya se ha eliminado del grupo. Entonces se asigna ese puntero
						//al array del metodo de la primera sesion valida que se encuentra en el grupo
						curBssSelMethod = groups[index_group].sessions[j].Bss_selected_method;
					}

					if (SipAgent::Coresip_Local_Config._Debug_BSS)
					{
						PJ_LOG(3,(__FILE__, "BSS: SetBetterSession FR %s %s qidx %d", groups[index_group].RdFr, 
							groups[index_group].sessions[j].pSipcall->DstUri, bss_sync));
					}

					if ((strcmp(groups[index_group].sessions[j].Bss_selected_method, curBssSelMethod) == 0) &&
						(bss_sync > better_bss))
					{
						better_index_sess = j;
						better_bss = bss_sync;
					}
				}

				if (groups[index_group].sessions[j].pSipcall->_Sending_Multicast_enabled)
					sessions_with_multicast_enabled_count++;	
			}
		}
	}
		
	pj_mutex_unlock(fd_mutex);
	
	if (better_bss > current_Bss_value)
	{
		//Existe otra sesion en el grupo con mejor bss. Lo seleccionamos.
		SipCall *p_better_sipcall = groups[index_group].sessions[better_index_sess].pSipcall;

		if ((!only_selected_in_window) ||
			(only_selected_in_window && (strcmp(groups[index_group].SelectedUri, p_better_sipcall->DstUri) == 0)))
		{
			//Si only_selected_in_window es false, quiere decir que se puede seleccionar una sesion que 
			//no sea la que se haya seleccionado en la ventana
			//Si only_selected_in_window es true, entonces solo se puede hacer la seleccion si la sesion
			//con mejor qidx es la que se haya seleccionado ya en la ventana

			EnableMulticast(p_better_sipcall, PJ_TRUE, PJ_FALSE);
			SetSelected(p_better_sipcall, PJ_TRUE, PJ_FALSE);
			if (in_window)
			{
				//Solo si estamos en la ventana se puede asignar en el grupo la uri seleccionada				
				SetSelectedUri(p_better_sipcall);
			}

			if (SipAgent::Coresip_Local_Config._Debug_BSS)
			{
				PJ_LOG(3,(__FILE__, "BSS: FR %s %s SELECCIONADO", groups[index_group].RdFr, p_better_sipcall->DstUri));
			}
		}
	}	
	else if (sessions_with_multicast_enabled_count == 0)
	{
		//La sesion con mejor bss es la actual. Sin embargo no esta seleccionada.
		//Posiblemente porque la transmision de audio durante la ventana no esta 
		//activada y la ventana ha vencido. Como no se ha encontrado ninguna sesion con mejor bss entonces se selecciona la actual
		//a no ser que no tenga el squelch activado, en cuyo caso se selecciona la sesion con mejor bss de las que tienen
		//el squelch activado.
		if (current_squ_status)
		{
			//Tiene el squelch activado

			if ((!only_selected_in_window) ||
				(only_selected_in_window && (strcmp(groups[index_group].SelectedUri, p_current_sipcall->DstUri) == 0)))
			{
				//Si only_selected_in_window es false, quiere decir que se puede seleccionar una sesion que 
				//no sea la que se haya seleccionado en la ventana
				//Si only_selected_in_window es true, entonces solo se puede hacer la saleccion si esta sesion
				//es la que se haya seleccionado ya en la ventana

				EnableMulticast(p_current_sipcall, PJ_TRUE, PJ_FALSE);
				SetSelected(p_current_sipcall, PJ_TRUE, PJ_FALSE);
				if (in_window)
				{
					//Solo si estamos en la ventana se puede asignar en el grupo la uri seleccionada
					SetSelectedUri(p_current_sipcall);
				}

				if (SipAgent::Coresip_Local_Config._Debug_BSS)
				{
					PJ_LOG(3,(__FILE__, "BSS: FR %s %s SELECCIONADO", groups[index_group].RdFr, p_current_sipcall->DstUri));
				}
			}
		}
		else if (better_index_sess != -1)
		{
			//No tiene el squelch activado y hay otra sesion con el mejor qidx, la cual es la que seleccionaremos

			if ((!only_selected_in_window) ||
				(only_selected_in_window && 
				 (strcmp(groups[index_group].SelectedUri, groups[index_group].sessions[better_index_sess].pSipcall->DstUri) == 0)))
			{
				//Si only_selected_in_window es false, quiere decir que se puede seleccionar una sesion que 
				//no sea la que se haya seleccionado en la ventana
				//Si only_selected_in_window es true, entonces solo se puede hacer la seleccion si la sesion
				//con mejor qidx es la que se haya seleccionado ya en la ventana

				EnableMulticast(groups[index_group].sessions[better_index_sess].pSipcall, PJ_TRUE, PJ_FALSE);
				SetSelected(groups[index_group].sessions[better_index_sess].pSipcall, PJ_TRUE, PJ_FALSE);
				if (in_window)
				{
					//Solo si estamos en la ventana se puede asignar en el grupo la uri seleccionada
					SetSelectedUri(groups[index_group].sessions[better_index_sess].pSipcall);
				}
				if (SipAgent::Coresip_Local_Config._Debug_BSS)
				{
					PJ_LOG(3,(__FILE__, "BSS: FR %s %s SELECCIONADO", groups[index_group].RdFr, 
						groups[index_group].sessions[better_index_sess].pSipcall->DstUri));
				}
			}
		}
	}

	//Refrescamos en el nodebox el estado de los canales del grupo
	for (j = 0; j < MAX_SESSIONS; j++)
	{
		if (groups[index_group].sessions[j].sess_callid != PJSUA_INVALID_ID &&
			groups[index_group].sessions[j].pSipcall != NULL)
		{
			CORESIP_CallFlags call_flags = groups[index_group].sessions[j].Flags;
			if (!(call_flags & CORESIP_CALL_RD_TXONLY))
			{
				pjmedia_session* ses = pjsua_call_get_media_session(groups[index_group].sessions[j].sess_callid);
				if (ses != NULL)
				{
					void * call = pjmedia_session_get_user_data(ses);
					if (call)
					{
						pj_mutex_lock(groups[index_group].sessions[j].pSipcall->RdInfo_prev_mutex);
						groups[index_group].sessions[j].pSipcall->RdInfo_prev.rx_selected = groups[index_group].sessions[j].selected;
						groups[index_group].sessions[j].pSipcall->RdInfo_prev.Squelch = groups[index_group].sessions[j].pSipcall->squ_status;
						if (groups[index_group].sessions[j].pSipcall->squ_status == 0)
						{
							groups[index_group].sessions[j].pSipcall->RdInfo_prev.rx_qidx = 0;
						}
						CORESIP_RdInfo info_aux;
						memcpy(&info_aux, &groups[index_group].sessions[j].pSipcall->RdInfo_prev, sizeof(CORESIP_RdInfo));
						pj_mutex_unlock(groups[index_group].sessions[j].pSipcall->RdInfo_prev_mutex);

						if (groups[index_group].sessions[j].pSipcall->Ptt_off_timer.id == 0)
						{

							//Actualizamos los parametros que no se toman en la callback OnRdInfochanged
							//Esto hay que hacerlo siempre que se llame a RdInfoCb
							//-->
							info_aux.rx_selected = SipAgent::_FrecDesp->IsBssSelected(groups[index_group].sessions[j].pSipcall);
							//<--

							//Solo se envia al nodebox fuera del timer de ptt off

							PJ_LOG(5,(__FILE__, "SetBetterSession: envia nodebox. dst %s PttType %d PttId %d rx_selected %d Squelch %d", 
							groups[index_group].sessions[j].pSipcall->DstUri, 
							info_aux.PttType, 
							info_aux.PttId, 
							info_aux.rx_selected, 
							info_aux.Squelch));

							if (SipAgent::Cb.RdInfoCb)
							{	
								SipAgent::Cb.RdInfoCb(((pjsua_call*)call)->index | CORESIP_CALL_ID, &info_aux);
							}
						}
					}
				}
			}
		}
	}

	return 0;
}



/**
 * 
 */
int FrecDesp::RefressStatus(SipCall *p_current_sipcall)
{
	int j;

	if (p_current_sipcall == NULL) 
	{
		return -1;
	}

	int index_group = p_current_sipcall->_Index_group;

	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return -1;
	}

	//Refrescamos en el nodebox el estado de los canales del grupo
	for (j = 0; j < MAX_SESSIONS; j++)
	{
		if (groups[index_group].sessions[j].sess_callid != PJSUA_INVALID_ID &&
			groups[index_group].sessions[j].pSipcall != NULL)
		{
			CORESIP_CallFlags call_flags = groups[index_group].sessions[j].Flags;
			if (!(call_flags & CORESIP_CALL_RD_TXONLY))
			{
				pjmedia_session* ses = pjsua_call_get_media_session(groups[index_group].sessions[j].sess_callid);
				if (ses != NULL)
				{
					void * call = pjmedia_session_get_user_data(ses);
					if (call)
					{
						pj_mutex_lock(groups[index_group].sessions[j].pSipcall->RdInfo_prev_mutex);
						groups[index_group].sessions[j].pSipcall->RdInfo_prev.rx_selected = groups[index_group].sessions[j].selected;
						groups[index_group].sessions[j].pSipcall->RdInfo_prev.Squelch = groups[index_group].sessions[j].pSipcall->squ_status;
						if (groups[index_group].sessions[j].pSipcall->squ_status == 0)
						{
							groups[index_group].sessions[j].pSipcall->RdInfo_prev.rx_qidx = 0;
						}
						CORESIP_RdInfo info_aux;
						memcpy(&info_aux, &groups[index_group].sessions[j].pSipcall->RdInfo_prev, sizeof(CORESIP_RdInfo));
						pj_mutex_unlock(groups[index_group].sessions[j].pSipcall->RdInfo_prev_mutex);

						if (groups[index_group].sessions[j].pSipcall->Ptt_off_timer.id == 0)
						{

							//Actualizamos los parametros que no se toman en la callback OnRdInfochanged
							//Esto hay que hacerlo siempre que se llame a RdInfoCb
							//-->
							info_aux.rx_selected = SipAgent::_FrecDesp->IsBssSelected(groups[index_group].sessions[j].pSipcall);
							//<--

							//Solo se envia al nodebox fuera del timer de ptt off

							PJ_LOG(5,(__FILE__, "RefressStatus: envia nodebox. dst %s PttType %d PttId %d rx_selected %d Squelch %d", 
							groups[index_group].sessions[j].pSipcall->DstUri, 
							info_aux.PttType, 
							info_aux.PttId, 
							info_aux.rx_selected, 
							info_aux.Squelch));

							if (SipAgent::Cb.RdInfoCb)
							{			
								SipAgent::Cb.RdInfoCb(((pjsua_call*)call)->index | CORESIP_CALL_ID, &info_aux);
							}
						}
					}
				}
			}
		}
	}

	return 0;
}

/**
 * EnableMulticast.	...
 * Activa el envio de multicast en una sesion del grupo y desactiva el resto o viceversa
 * @param	psipcall	Puntero al objeto que maneja la sesion.
 * @param	enable		Si true, entonces activa el envio multicast, si false lo desactiva 
 * @param	all			Si true realiza la acci�n en todas las sesiones del mismo grupo. Si false realiza la accion s�lo a esa sesi�n
 *						y la contraria al resto
 * @return	-1 su hay error.
 */
int FrecDesp::EnableMulticast(SipCall *psipcall, pj_bool_t enable, pj_bool_t all)
{
	if (psipcall == NULL) return -1;

	int index_group = psipcall->_Index_group;
	int index_sess = psipcall->_Index_sess;
	pj_bool_t not_enable;

	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return -1;
	}

	if (enable) not_enable = PJ_FALSE;
	else not_enable = PJ_TRUE;

	pj_mutex_lock(fd_mutex);
	for (int j = 0; (j < MAX_SESSIONS); j++)
	{
		if (groups[index_group].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
		if (!pjsua_call_is_active(groups[index_group].sessions[j].sess_callid)) continue;
		if (!pjsua_call_has_media(groups[index_group].sessions[j].sess_callid)) continue;

		if (j == index_sess && groups[index_group].sessions[j].pSipcall != NULL && !all)
		{
			groups[index_group].sessions[j].pSipcall->_Sending_Multicast_enabled = enable;
		}
		else if (!all)
		{
			groups[index_group].sessions[j].pSipcall->_Sending_Multicast_enabled = not_enable;
		}
		else
		{
			groups[index_group].sessions[j].pSipcall->_Sending_Multicast_enabled = enable;
		}
	}
	pj_mutex_unlock(fd_mutex);

	return 0;
}

/**
 * SetSelected.	...
 * Pone a true la sesi�n seleccionada. 
 * @param	psipcall	Puntero al objeto que maneja la sesi�n.
 * @param	selected	Si true, indica que esta seleccionada, 
 * @param	all			Si true realiza la acci�n en todas las sesiones del mismo grupo. Si false realiza la accion s�lo a esa sesi�n
 *						y la contraria al resto
 * @return	-1 su hay error.
 */
int FrecDesp::SetSelected(SipCall *psipcall, pj_bool_t selected, pj_bool_t all)
{
	if (psipcall == NULL) return -1;

	int index_group = psipcall->_Index_group;
	int index_sess = psipcall->_Index_sess;
	pj_bool_t not_selected;

	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return -1;
	}

	if (selected) not_selected = PJ_FALSE;
	else not_selected = PJ_TRUE;

	pj_mutex_lock(fd_mutex);
	for (int j = 0; (j < MAX_SESSIONS); j++)
	{
		if (groups[index_group].sessions[j].sess_callid == PJSUA_INVALID_ID) continue;
		if (!pjsua_call_is_active(groups[index_group].sessions[j].sess_callid)) continue;
		if (!pjsua_call_has_media(groups[index_group].sessions[j].sess_callid)) continue;

		if (j == index_sess && groups[index_group].sessions[j].pSipcall != NULL && !all)
		{
			groups[index_group].sessions[j].selected = selected;
		}
		else if (!all)
		{
			groups[index_group].sessions[j].selected = not_selected;
		}
		else
		{
			groups[index_group].sessions[j].selected = selected;
		}
	}
	pj_mutex_unlock(fd_mutex);

	return 0;
}

/**
 * SetSelectedUri.	...
 * Pone en el grupo la uri de la mejor session que se ha seleccionado. 
 * @param	psipcall	Puntero de la sipcall seleccionada.
 * @return	-1 si hay error.
 */
int FrecDesp::SetSelectedUri(SipCall *psipcall)
{
	if (psipcall == NULL) return -1;

	int index_group = psipcall->_Index_group;
	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return -1;
	}

	pj_mutex_lock(fd_mutex);
	strncpy(groups[index_group].SelectedUri, psipcall->DstUri, sizeof(groups[index_group].SelectedUri));
	groups[index_group].SelectedUri[sizeof(groups[index_group].SelectedUri)-1] = '\0';
	groups[index_group].SelectedUriPttId = psipcall->RdInfo_prev.PttId;
	pj_mutex_unlock(fd_mutex);

	return 0;
}

/**
 * GetSelectedUri.	...
 * Retorna la uri de la seleccionada y el Ptt-Id de cuando se selecciono
 * @param	psipcall			Puntero al objeto que maneja la sesion.
 * @param	selectedUri			retorna el puntero de la Uri
 * @param	selectedUriPttId	retorna el valor del ptt-id
 * @return	-1 si hay error.
 */
int FrecDesp::GetSelectedUri(SipCall *psipcall, char **selectedUri, unsigned short *selectedUriPttId)
{
	if (psipcall == NULL) return -1;

	int index_group = psipcall->_Index_group;
	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return -1;
	}

	pj_mutex_lock(fd_mutex);
	if (selectedUri != NULL)
	{
		*selectedUri = groups[index_group].SelectedUri;
	}
	if (selectedUriPttId != NULL)
	{
		*selectedUriPttId = groups[index_group].SelectedUriPttId;
	}
	pj_mutex_unlock(fd_mutex);

	return 0;
}

/**
 * ClrSelectedUri.	... 
 * @return	-1 si hay error.
 */
int FrecDesp::ClrSelectedUri(SipCall *psipcall)
{
	if (psipcall == NULL) return -1;

	int index_group = psipcall->_Index_group;
	if (index_group < 0 || index_group >= MAX_GROUPS) 
	{
		return -1;
	}

	pj_mutex_lock(fd_mutex);
	groups[index_group].SelectedUri[0] = '\0';
	groups[index_group].SelectedUriPttId = 0;
	pj_mutex_unlock(fd_mutex);

	return 0;
}

/**
 * IsBssSelected.	...
 * Retorna si la sesi�n es la seleccionada.
 * @param	psipcall	Puntero al objeto que maneja la sesi�n.
 * @return	true si es el seleccionado.
 */
pj_bool_t FrecDesp::IsBssSelected(SipCall *psipcall)
{
	if (psipcall == NULL) return PJ_FALSE;

	int index_group = psipcall->_Index_group;
	int index_sess = psipcall->_Index_sess;
	pj_bool_t ret;
	
	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return PJ_FALSE;
	}
	
	pj_mutex_lock(fd_mutex);
	ret = groups[index_group].sessions[index_sess].selected;
	pj_mutex_unlock(fd_mutex);

	return ret;
}

/**
 * IsValidTdTx.	...
 * Retorna Si el tiempo de retardo calculado es valido.
 * @param	psipcall	Puntero al objeto que maneja la sesi�n.
 * @return	true si es valido.
 */
pj_bool_t FrecDesp::IsValidTdTxIP(SipCall *psipcall)
{
	if (psipcall == NULL) return PJ_FALSE;

	int index_group = psipcall->_Index_group;
	int index_sess = psipcall->_Index_sess;
	pj_bool_t ret = PJ_FALSE;
	
	if (index_group < 0 || index_sess < 0 || index_group >= MAX_GROUPS || index_sess >= MAX_SESSIONS) 
	{
		return ret;
	}
	
	pj_mutex_lock(fd_mutex);
	if (groups[index_group].sessions[index_sess].TdTxIP != INVALID_TIME_DELAY)
		ret = PJ_TRUE;
	pj_mutex_unlock(fd_mutex);

	return ret;
}

/**
 * ConectNTPd.	...
 * Conecta socket a ntpd.
 * @return	-1 si hay error. 0 OK
 */
int FrecDesp::ConnectNTPd(char *err, int err_size)
{
	pj_sockaddr_in sock;
	struct pj_in_addr   address;
	const int NTP_PORT = 123;

	/* initialise file descriptor set */
	PJ_FD_ZERO(&ntp_fds);

	pj_status_t st = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &ntp_sd);
	if (st != PJ_SUCCESS)
	{
		ntp_sd = PJ_INVALID_SOCKET;
		strncpy(err, "ERROR: NtpStat. No se puede abrir el socket", err_size);
		err[err_size-1] = 0;
		return -1;
	}

	pj_inet_aton(&pj_str("127.0.0.1"), &address);
	sock.sin_family = AF_INET;
	sock.sin_addr = address;
	sock.sin_port = pj_htons(NTP_PORT);

	st = pj_sock_connect(ntp_sd, &sock, sizeof(sock));
	if (st != PJ_SUCCESS)
	{
		pj_sock_close(ntp_sd);
		ntp_sd = PJ_INVALID_SOCKET;
		strncpy(err, "ERROR: NtpStat. El socket no se puede conectar", err_size);
		err[err_size-1] = 0;
		return -1;
	}
		
	PJ_FD_SET(ntp_sd, &ntp_fds);
	return 0;
}

/**
 * ConectNTPd.	...
 * Desconecta socket a ntpd.
 * @return	-1 si hay error. 0 OK
 */
int FrecDesp::DisConnectNTPd(char *err, int err_size)
{
	const int NTP_PORT = 123;
		
	/* initialise file descriptor set */
	PJ_FD_ZERO(&ntp_fds);

	if (ntp_sd == PJ_INVALID_SOCKET) return 0;

	pj_status_t st = pj_sock_shutdown(ntp_sd, PJ_SHUT_RDWR);
	st = pj_sock_close(ntp_sd);
	ntp_sd = PJ_INVALID_SOCKET;	
	
	return 0;
}


/**
 * NtpStat.	...
 * @param err. Texto con el error 
 * @param err_size. Longitud del array
 */
/* This function uses an NTP mode 6 control message, which is the
   same as that used by the ntpq command.  
   For details on the format of NTP control message, see
   http://www.eecis.udel.edu/~mills/database/rfc/rfc1305/rfc1305b.ps.
   This document covers NTP version 2, however the control message
   format works with ntpd version 4 and earlier.
   Section 3.2 (pp 9 ff) of RFC1305b describes the data formats used by
   this program.
   This returns:
      0:   if clock is synchronised.
      1:   if clock is not synchronised.
      2:   if clock state cannot be determined, eg if
           ntpd is not contactable.                 */
int FrecDesp::NtpStat(char *err, int err_size)
{
	/* ------------------------------------------------------------------------*/
	/* value of Byte1 and Byte2 in the ntpmsg       */
	const unsigned char B1VAL = 0x16;   /* VN = 2, Mode = 6 */
	const unsigned char B2VAL = 2;      /* Response = 0; ( this makes the packet a command )
							Error    = 0;
							More     = 0;
							Op Code  = 2 (read variables command) */
	const unsigned char RMASK = 0x80;  /* bit mask for the response bit in Status Byte2 */
	const unsigned char EMASK = 0x40;  /* bit mask for the error bit in Status Byte2 */
	const unsigned char MMASK = 0x20;  /* bit mask for the more bit in Status Byte2 */
	const unsigned int PAYLOADSIZE = 468; /* size in bytes of the message payload string */
	/*-------------------------------------------------------------------------*/

	int					rc;        //  return code		
	pj_time_val			tv;
	int                 n;         /* number returned from select call */
	unsigned char       byte1ok;
	unsigned char       byte2ok;

	struct  {               /* RFC-1305 NTP control message format */
		unsigned char byte1;  /* Version Number: bits 3 - 5; Mode: bits 0 - 2; */
		unsigned char byte2;  /* Response: bit 7;
                             Error: bit 6;
                             More: bit 5;
                             Op code: bits 0 - 4 */
		unsigned short sequence;
		unsigned char  status1;  /* LI and clock source */
		unsigned char  status2;  /* count and code */
		unsigned short AssocID;
		unsigned short Offset;
		unsigned short Count;
		char payload[PAYLOADSIZE];
		char authenticator[96];
	} ntpmsg;

	char buff[PAYLOADSIZE];  /* temporary buffer holding payload string */

	unsigned int clksrc;
	char *clksrcname[] = {  /* Refer RFC-1305, Appendix B, Section 2.2.1 */
		"unspecified",    /* 0 */
		"atomic clock",   /* 1 */
		"VLF radio",      /* 2 */
		"HF radio",       /* 3 */
		"UHF radio",      /* 4 */
		"local net",      /* 5 */
		"NTP server",     /* 6 */
		"UDP/TIME",       /* 7 */
		"wristwatch",     /* 8 */
		"modem"};         /* 9 */
	char *newstr;
	char *dispstr;
	char *delaystr;
	const char DISP[] = "rootdisp=";
	const char DELAY[] = "rootdelay=";
	const char STRATUM[] = "stratum=";
	const char POLL[] = "tc=";
	const char REFID[] = "refid=";
	const char OFFSET[] = "offset=";

	/* initialise timeout value */
	tv.sec = 1;
	tv.msec = 0;

	if (err_size > 0) err[0] = '\0';
	strncpy(err, "NtpStat. OK", err_size);
	err[err_size-1] = 0;
	
	/*----------------------------------------------------------------*/
	/* Compose the command message */

	pj_memset ( &ntpmsg, 0, sizeof(ntpmsg));
	ntpmsg.byte1 = B1VAL;
	ntpmsg.byte2 = B2VAL;  
	ntpmsg.sequence = pj_htons(1);

	if (ntp_sd == PJ_INVALID_SOCKET)
	{
		if (ConnectNTPd(err, err_size) == -1)
		{
			return 2;
		}
	}

	/*---------------------------------------------------------------------*/
	/* Send the command message */

	pj_ssize_t pkgsize = (pj_ssize_t) sizeof(ntpmsg);
	pj_status_t st = pj_sock_send(ntp_sd, &ntpmsg, &pkgsize, 0);
	if (st != PJ_SUCCESS)
	{
		DisConnectNTPd(err, err_size);		//Nos desconectamos de NTPd 
		strncpy(err, "ERROR: NtpStat. No se puede enviar paquete al puerto NTP", err_size);
		err[err_size-1] = 0;	

		return 2;
	}

	/*----------------------------------------------------------------------*/
	/* Receive the reply message */

	n = pj_sock_select(ntp_sd+1, &ntp_fds, (pj_fd_set_t *) NULL, (pj_fd_set_t *) NULL, &tv);
	if (n == 0)
	{
		DisConnectNTPd(err, err_size);		//Nos desconectamos de NTPd
		strncpy(err, "ERROR: NtpStat. Timeout", err_size);
		err[err_size-1] = 0;
		return 2;
	}

	if (n == -1)
	{
		DisConnectNTPd(err, err_size);		//Nos desconectamos de NTPd
		strncpy(err, "ERROR: NtpStat. Error on select", err_size);
		err[err_size-1] = 0;
		return 2;
	}

	pkgsize = (pj_ssize_t) sizeof(ntpmsg);
	st = pj_sock_recv(ntp_sd,  &ntpmsg, &pkgsize, 0);
	if (st != PJ_SUCCESS)
	{
		DisConnectNTPd(err, err_size);		//Nos desconectamos de NTPd
		strncpy(err, "ERROR: NtpStat. Unable to talk to NTP daemon. Is it running?", err_size);
		err[err_size-1] = 0;
		return 2;
	}

	/*----------------------------------------------------------------------*/
	/* Interpret the received NTP control message */
	PJ_LOG(5,(__FILE__, "NtpStat. NTP mode 6 message"));
	PJ_LOG(5,(__FILE__, "NtpStat. cmd = %0x%0x",ntpmsg.byte1,ntpmsg.byte2));
	PJ_LOG(5,(__FILE__, "NtpStat. status = %0x%0x",ntpmsg.status1,ntpmsg.status2));
	PJ_LOG(5,(__FILE__, "NtpStat. AssocID = %0x",ntpmsg.AssocID));
	PJ_LOG(5,(__FILE__, "NtpStat. Offset = %0x",ntpmsg.Offset));
	PJ_LOG(5,(__FILE__, "NtpStat. %s/n",ntpmsg.payload));

	/* For the reply message to be valid, the first byte should be as sent,
		and the second byte should be the same, with the response bit set */
	byte1ok = ((ntpmsg.byte1 & 0x3F) == B1VAL);
	byte2ok = ((ntpmsg.byte2 & ~MMASK) == (B2VAL|RMASK));
	if (!(byte1ok && byte2ok)) {
		PJ_LOG(5,(__FILE__, "ERROR: NtpStat. status word is 0x%02x%02x", ntpmsg.byte1,ntpmsg.byte2 ));
		strncpy(err, "ERROR: NtpStat. return data appears to be invalid based on status word", err_size);
		err[err_size-1] = 0;
		return 2;
	}

	if (!(ntpmsg.byte2 | EMASK)) {
		PJ_LOG(5,(__FILE__, "ERROR: NtpStat. status byte2 is %02x", ntpmsg.byte2 ));
		strncpy(err, "ERROR: NtpStat. error bit is set in reply", err_size);
		err[err_size-1] = 0;
		return 2;
	}

	if (!(ntpmsg.byte2 | MMASK)) {
		PJ_LOG(5,(__FILE__, "WARNING: NtpStat. status byte2 is %02x", ntpmsg.byte2 ));
		PJ_LOG(5,(__FILE__, "WARNING: NtpStat. More bit unexpected in reply"));
	}

	/* if the leap indicator (LI), which is the two most significant bits
		in status byte1, are both one, then the clock is not synchronised. */
	if ((ntpmsg.status1 >> 6) == 3) {
		/* look at the system event code and see if indicates system restart */
		if ((ntpmsg.status2 & 0x0F) == 1)
			PJ_LOG(5,(__FILE__, "WARNING: NtpStat. time server re-starting"));

		strncpy(err, "ERROR: NtpStat. No sincronizado con servidor NTP", err_size);
		err[err_size-1] = 0;
		rc=1;
	}
	else {
		rc=0;

		clksrc = (ntpmsg.status1 & 0x3F);
		if (clksrc < 10)
			PJ_LOG(5,(__FILE__, "NtpStat. Sincronizado con %s", clksrcname[clksrc]));
		else
			PJ_LOG(5,(__FILE__, "NtpStat. Sincronizado con fuente desconocida"));

		if (clksrc == 6) {
			// source of sync is another NTP server so check the IP address
			strncpy(buff, ntpmsg.payload, sizeof(buff));
			if ((newstr = strstr (buff, REFID))) {
				newstr += sizeof(REFID) - 1;
				dispstr = strtok(newstr,",");

				/* Check the resultant string is of a reasonable length */
				if ((strlen (dispstr) == 0) || (strlen (dispstr) > 16)) {
					PJ_LOG(5,(__FILE__, "WARNING: NtpStat. Sincronizado con otro servidor NTP desconocido. La IP no es legible"));
				}
				else {
					PJ_LOG(5,(__FILE__, "WARNING: NtpStat. Sincronizado con otro servidor NTP (%s)", dispstr));
				}
			} else {
				rc=1;
				PJ_LOG(5,(__FILE__, "ERROR: NtpStat. Sincronizado con otro servidor NTP desconocido."));
				strncpy(err, "ERROR: NtpStat. Sincronizado con otro servidor NTP desconocido.", err_size);
				err[err_size-1] = 0;
			}

		}
		/* the message payload is an ascii string like
		   version="ntpd 4.0.99k Thu Apr  5 14:21:47 EDT 2001 (1)",
		   processor="i686", system="Linux2.4.2-2", leap=0, stratum=3,
		   precision=-17, rootdelay=205.535, rootdispersion=57.997, peer=22924,
		   refid=203.21.84.4, reftime=0xbedc2243.820c282c, poll=10,
		   clock=0xbedc2310.75708249, state=4, phase=0.787, frequency=19.022,
		   jitter=8.992, stability=0.029 */

		strncpy(buff, ntpmsg.payload, sizeof(buff));
		if ((newstr = strstr (buff, STRATUM))) {
			newstr += sizeof(STRATUM) - 1;
			dispstr = strtok(newstr,",");

			/* Check the resultant string is of a reasonable length */
			if ((strlen (dispstr) == 0) || (strlen (dispstr) > 2)) {
				PJ_LOG(5,(__FILE__, "WARNING: NtpStat. Stratum no es legible"));
			}
			else {
				PJ_LOG(5,(__FILE__, "NtpStat. Stratum %s", dispstr));
			}
		} else {
			rc=1;
			PJ_LOG(5,(__FILE__, "ERROR: NtpStat. Stratum desconocido"));
			strncpy(err, "ERROR: NtpStat. Stratum desconocido.", err_size);
			err[err_size-1] = 0;
		}

		/* Set the position of the start of the string to
			"rootdispersion=" part of the string. */
		strncpy(buff, ntpmsg.payload, sizeof(buff));
		if ((dispstr = strstr (buff, DISP)) && (delaystr = strstr (buff, DELAY))) {
			dispstr += sizeof(DISP) - 1;
			dispstr = strtok(dispstr,",");
			delaystr += sizeof(DELAY) - 1;
			delaystr = strtok(delaystr,",");

			/* Check the resultant string is of a reasonable length */
			if ((strlen (dispstr) == 0) || (strlen (dispstr) > 10) ||
				(strlen (delaystr) == 0) || (strlen (delaystr) > 10)) {
				PJ_LOG(5,(__FILE__, "WARNING: NtpStat. Precision no legible"));
			}
			else {
				PJ_LOG(5,(__FILE__, "NtpStat.  time correct to within %.0f ms", atof(dispstr) + atof(delaystr) / 2.0));
			}
		} else {
			rc=1;
			PJ_LOG(5,(__FILE__, "ERROR: NtpStat. Precision desconocida"));
			strncpy(err, "ERROR: NtpStat. Precision desconocida.", err_size);
			err[err_size-1] = 0;
		}
	}

	strncpy(buff, ntpmsg.payload, sizeof(buff));
	if ((newstr = strstr (buff, POLL))) {
		newstr += sizeof(POLL) - 1;
		dispstr = strtok(newstr,",");

		/* Check the resultant string is of a reasonable length */
		if ((strlen (dispstr) == 0) || (strlen (dispstr) > 2)) {
			PJ_LOG(5,(__FILE__, "WARNING: NtpStat. poll interval unreadable"));
		}
		else {
			PJ_LOG(5,(__FILE__, "NtpStat. polling server every %d s",1 << atoi(dispstr)));
		}
	} else {
		rc=1;
		PJ_LOG(5,(__FILE__, "ERROR: NtpStat. poll interval unknown"));
		strncpy(err, "ERROR: NtpStat. poll interval unknown.", err_size);
		err[err_size-1] = 0;
	}

	float foffset = 100.0;
	strncpy(buff, ntpmsg.payload, sizeof(buff));
	if ((newstr = strstr (buff, OFFSET))) {
		newstr += sizeof(OFFSET) - 1;
		dispstr = strtok(newstr,",");

		/* Check the resultant string is of a reasonable length */
		if ((strlen (dispstr) == 0) || (strlen (dispstr) > 10)) {
			rc=1;
			PJ_LOG(5,(__FILE__, "ERROR: NtpStat. offset unreadable"));
			strncpy(err, "ERROR: NtpStat. offset unreadable.", err_size);
			err[err_size-1] = 0;
		}
		else {
			foffset = (float) atof(dispstr);			 
			PJ_LOG(5,(__FILE__, "NtpStat. offset=%f",foffset));
			if (fabs(foffset) > OFFSET_THRESHOLD)
			{
				//Si el valor de offset es mayor de 1ms entonces no se considera sincronizado
				rc = 1;
				PJ_LOG(5,(__FILE__, "ERROR: NtpStat. No sincronizado. offset=%f > 1ms o < -1ms", foffset));
				strncpy(err, "ERROR: NtpStat. No sincronizado. offset > 1ms o < -1ms.", err_size);
				err[err_size-1] = 0;
			}
		}
	} else {
		rc=1;
		PJ_LOG(5,(__FILE__, "ERROR: NtpStat. offset unknown"));
		strncpy(err, "ERROR: NtpStat. offset unknown.", err_size);
		err[err_size-1] = 0;
	}

	return rc;
}

/**
 * NTPCheckTh.	...
 * Tarea que chequea la sincronizacion NTP.
 * @return	retorno de la tarea.
 */
int FrecDesp::NTPCheckTh(void *proc)
{
	FrecDesp *wp = (FrecDesp *)proc;
	int pos = 0;	

	pj_thread_desc desc;
    pj_thread_t *this_thread;
	pj_status_t rc;
	unsigned int mysleep = 250;
	int ntries = 20;
	unsigned int multiplier = 1;

	char ntp_error[512];
	char ntp_error_prev[512];

	pj_bzero(ntp_error, sizeof(ntp_error));
	pj_bzero(ntp_error_prev, sizeof(ntp_error_prev));
	pj_bzero(desc, sizeof(desc));

    rc = pj_thread_register("NTPCheckTh", desc, &this_thread);
    if (rc != PJ_SUCCESS) {
		PJ_LOG(3,(__FILE__, "...error in pj_thread_register NTPCheckTh!"));
        return NULL;
    }

    /* Test that pj_thread_this() works */
    this_thread = pj_thread_this();
    if (this_thread == NULL) {
        PJ_LOG(3,(__FILE__, "...error: NTPCheckTh pj_thread_this() returns NULL!"));
        return NULL;
    }

    /* Test that pj_thread_get_name() works */
    if (pj_thread_get_name(this_thread) == NULL) {
        PJ_LOG(3,(__FILE__, "...error: NTPCheckTh pj_thread_get_name() returns NULL!"));
        return NULL;
    }


	while (wp->ntp_check_thread_run)
	{
		if (wp->ngroups == 0)
		{
			//Si no hay ningun grupo creado, entonces no hacemos nada en este hilo
			for (unsigned int i = 0; i < ((PERIOD_CHECK_NTP*multiplier)/mysleep); i++)
			{
				pj_thread_sleep(mysleep);
				if (!wp->ntp_check_thread_run) continue;
			}
			continue;
		}

		pj_bool_t ntp_sync = PJ_FALSE;

		if (wp->NtpStat(ntp_error, sizeof(ntp_error)) == 0) ntp_sync = PJ_TRUE;
		if (strcmp(ntp_error, ntp_error_prev) != 0)
		{
			//Solamente se produce un LOG si el error retornado cambia. Para no inundar el log
			PJ_LOG(3,(__FILE__, ntp_error));
			strcpy(ntp_error_prev, ntp_error);
		}

		if (wp->NTP_synchronized != ntp_sync)		
		{
			wp->NTP_synchronized = ntp_sync;			
			if (ntp_sync)
			{
				PJ_LOG(3,(__FILE__, "Sincronizacion con servidor NTP: OK"));
			}
			else
			{
				PJ_LOG(3,(__FILE__, "ERROR: No hay sincronizacion con servidor NTP"));
			}			
		}	

		wp->UpdateGroupClimaxParamsAllGroups();

		for (unsigned int i = 0; i < ((PERIOD_CHECK_NTP*multiplier)/mysleep); i++)
		{
			pj_thread_sleep(mysleep);
			if (!wp->ntp_check_thread_run) continue;
		}
	}

	return 0;
}


