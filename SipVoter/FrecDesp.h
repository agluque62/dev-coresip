/**
 * @file FrecDesp.h
 * @brief Gestion de Frecuencias Desplazadas en CORESIP.dll
 *
 *	Implementa la clase 'FrecDesp'.
 *
 *	@addtogroup CORESIP
 */
/*@{*/

#ifndef __CORESIP_FRECDESP_H__
#define __CORESIP_FRECDESP_H__

#include "CoreSip.h"
#include "SipCall.h"

class FrecDesp
{
public:

	static const int INVALID_GROUP_INDEX = -1;
	static const int INVALID_SESS_INDEX = -1;
	static const int MAX_GROUPS = 1024;   //maximo numero de grupos
	static const int MAX_SESSIONS = 10;	 //maximo numero de sesiones en un grupo

	static const pj_bool_t IN_WINDOW = PJ_TRUE;	 //Indica que se está en la ventana de decision bss
	static const pj_bool_t ONLY_SELECTED_IN_WINDOW = PJ_TRUE;	 //Indica que solo se selecciona la que fue mejor en la ventana de decision

	pj_bool_t NTP_synchronized;			
	
	FrecDesp();
	~FrecDesp();
	
	int AddToGroup(char *rdfr, pjsua_call_id call_id, SipCall *psipcall, char *Bss_selected_method, CORESIP_CallFlags flags);
	int RemFromGroup(int index_group, int index_sess);
	int SetGroupClimaxFlag(int index_group, pj_bool_t Esgrupoclimax);
	int GetGroupClimaxFlag(pjsua_call_id call_id, pj_bool_t *Esgrupoclimax);
	int GetSessionsCountInGroup(int index_group, int *nsessions_rx_only, int *nsessions_tx_only);
	int SetTimeDelay(pjmedia_stream *stream, CORESIP_PttType ptttype, int index_group, int index_sess, pj_uint8_t *ext_value, pj_bool_t *request_MAM);
	pj_uint32_t GetRetToApply(int index_group, int index_sess);
	int GetCLD(pjsua_call_id call_id, pj_uint8_t *cld);
	int SetBss(int index_group, int index_sess, pj_uint8_t *ext_value, int *BssMethod, int *BssValue);
	int SetBss(int index_group, int index_sess, pj_uint8_t qidx_method, pj_uint8_t qidx_value);
	int GetBss(int index_group, int index_sess, int *BssMethod, int *BssValue);
	void GetQidx(int index_group, int index_sess, pj_uint8_t *qidx, pj_uint8_t *qidx_ml, pj_uint32_t *Tred);
	int SetSquSt(int index_group, int index_sess, pj_bool_t squ_st, int *sq_air_count);
	int ClrAllSquSt(int index_group);
	int GetSquCnt(int index_group);
	int SetBetterSession(SipCall *p_current_sipcall, pj_bool_t in_window, pj_bool_t only_selected_in_window);
	int EnableMulticast(SipCall *psipcall, pj_bool_t enable, pj_bool_t all);
	int SetSelected(SipCall *psipcall, pj_bool_t selected, pj_bool_t all);
	int SetSelectedUri(SipCall *psipcall);
	int ClrSelectedUri(SipCall *psipcall);
	int Set_mcast_seq(int index_group, unsigned mcast_seq);
	unsigned Get_mcast_seq(int index_group);
	int Set_group_multicast_socket(int index_group, pj_sockaddr_in *RdSendTo);
	int Get_group_multicast_socket(int index_group, pj_sockaddr_in **RdSendTo);
	pj_bool_t IsBssSelected(SipCall *psipcall);
	pj_bool_t IsValidTdTxIP(SipCall *psipcall);
	int GetLastCld(int index_group, int index_sess, pj_uint8_t *cld);
	int RefressStatus(SipCall *p_current_sipcall);
	int SetInWindow(int index_group, pj_bool_t status);
	pj_bool_t GetInWindow(int index_group, int index_sess);
	int GetSelectedUri(SipCall *psipcall, char **selectedUri, unsigned short *selectedUriPttId);
		
private:

	static const unsigned int PERIOD_CHECK_NTP = 3000;	
	static const pj_uint32_t Tv1 = 0;
	static const pj_uint32_t Tp1 = 160;		//20 ms en unidades de 125us
	static const float OFFSET_THRESHOLD;	//Umbral de valided del Offset del ntp en milisegundos
	static const pj_uint32_t ui32_OFFSET_THRESHOLD_us = 2000;		//Umbral de valided del Offset del ntp en microsegundos
	static const pj_uint32_t INVALID_TIME_DELAY = 0xFFFFFFFF;
	static const unsigned long long INVALID_TIME_SQU = 0;
	static const pj_uint32_t INVALID_TIME_FSQU = 0xFFFFFFFF;
	static const pj_uint32_t INVALID_CLD_PREV = 0xFFFFFFFF;
	static const int Tn1_count_MAX = 1;
	static const int Tj1_count_MAX = 1;		

	pj_pool_t * _Pool;
	pj_bool_t ntp_check_thread_run;

	pj_thread_t  *ntp_check_thread;
	static pj_thread_proc NTPCheckTh;

	pj_mutex_t *fd_mutex;

	struct stgrupo
	{
		char RdFr[CORESIP_MAX_RS_LENGTH + 1];			//Frecuencia que identifica al grupo.	
		char Zona[CORESIP_MAX_ZONA_LENGTH + 1];			//Frecuencia que identifica al grupo.
		int nsessions;									//Cantidad total de sesiones
		int nsessions_tx_only;							//Cantidad de sesiones Tx only
		int nsessions_rx_only;							//Cantidad de sesiones Rx only

		struct stsess
		{
			pjsua_call_id sess_callid;		//call ids de las sesiones que est�n dentro del grupo.
			SipCall *pSipcall;				//Puntero al objeto del tipo SipCall que maneja la sesion
			pj_uint32_t TdTxIP;			    //Time delay calculado para la radio de cada sesion, despues de recibir MAM
											//En unidades de 125 us
			pj_uint32_t Tj1;				//Ultimo Tj1 obtenido
			int Tj1_count;					//Cantidad de veces que el retardo es distinto al anterior
			pj_uint32_t cld_prev;			//Ultimo cld obtenido. En 32 bits
			pj_bool_t cld_absoluto;			//Indica si el metodo CLD es el absoluto o no
			int Tn1_count;					//Cantidad de veces que el retardo es distinto al anterior
			pj_uint32_t Tred;				//Ultimo retardo de red obtenido. En unidades de 125 us
			pj_uint32_t Tid_orig;			//Tid obtenido en el primer MAM despues de que la sesion ha sido establecida o cuando NMR se recibe
			pj_uint8_t Bss_value;			//Valor del bss recibido
			pj_uint8_t Bss_type;			//Tipo del bss recibido
			pj_bool_t squ_status;			//Estado del squelch. 
			pj_bool_t selected;				//Indica si es el seleccionado	
			char Bss_selected_method[64];	//Metodo bss seleccionado por el GRS. Es el literal recibido en el sdp		
			CORESIP_CLD_CALCULATE_METHOD _MetodoClimax;
			CORESIP_CallFlags Flags;
			pj_bool_t in_window_timer;		//Indica que estamos en la ventana de decision del bss
			int PesoRSSIvsNucleo;			//Peso del valor de Qidx del tipo RSSI en el calculo del Qidx final. 0 indica que el calculo es interno (centralizado). 9 que el calculo es solo el RSSI.
			
		} sessions[MAX_SESSIONS];

		char SelectedUri[CORESIP_MAX_URI_LENGTH + 1];   //Uri del receptor seleccionado en la ventana BSS
		unsigned short SelectedUriPttId;				//PTT-Id de la uri seleccionada
		unsigned mcast_seq;					//N�mero de secuencia que se env�a con el paquete de audio por multicast
		pj_sockaddr_in *_RdSendTo;			//Direcci�n y puerto multicast donde se env�a. Lo asigna la sesi�n que primero
											//active el squelch

	} groups[MAX_GROUPS];

	int ngroups;
		
	int UpdateGroupClimaxParams(int index_group);
	int UpdateGroupClimaxParamsAllGroups();
	int GetNextLineField(int pos, char *line, char *out, int out_size);

	pj_sock_t ntp_sd;						//Socket descriptor para conectarse a ntpd       
	pj_fd_set_t ntp_fds;					
	int ConnectNTPd(char *err, int err_size);	
	int DisConnectNTPd(char *err, int err_size);
	int NtpStat(char *err, int err_size);
};


#endif
