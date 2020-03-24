#include "Global.h"
#include "Exceptions.h"
#include "Guard.h"
#include "SipAgent.h"
#include "RecordPort.h"

//Comandos al grabador
const char *RecordPort::NOTIF_IPADD = "V,I00,";
const char *RecordPort::INI_SES_REC_TERM = "V,T00,";
const char *RecordPort::FIN_SES_REC_TERM = "V,T01,";
const char *RecordPort::INI_SES_REC_RAD = "V,G00,";
const char *RecordPort::FIN_SES_REC_RAD = "V,G01,";
const char *RecordPort::REMOVE_REC_OBJ = "V,DDD,";
const char *RecordPort::REC_INV = "V,INV,";
const char *RecordPort::REC_BYE = "V,BYE,";
const char *RecordPort::SQUELCH_ON = "V,G02,";
const char *RecordPort::SQUELCH_OFF = "V,G03,";
const char *RecordPort::REC_BSS = "V,G06,";			//Seleccion de mejor recurso por el BSS
const char *RecordPort::REC_CALLSTART = "V,T02,";
const char *RecordPort::REC_CALLEND = "V,T03,";
const char *RecordPort::REC_CALLCONNECTED = "V,T04,";
const char *RecordPort::REC_PTTON = "V,T20,";
const char *RecordPort::REC_PTTOFF = "V,T21,";
const char *RecordPort::REC_HOLDON = "V,T08,";
const char *RecordPort::REC_HOLDOFF = "V,T09,";
const char *RecordPort::REC_RECORD = "V,I01,";
const char *RecordPort::REC_PAUSE = "V,I02,";
const char *RecordPort::REC_RESET = "C,H02";
const char *RecordPort::REC_NOT_MESSAGE = "NOMES";

//Respuestas del grabador
const char *RecordPort::RESPOK = "G,E00,0";
const char *RecordPort::BADDIRIP = "G,E00,1";
const char *RecordPort::NOT_ACTIVE_SESSION = "G,E00,2";
const char *RecordPort::OVRFLOW = "G,E00,3";
const char *RecordPort::COMMAND_ERROR = "G,E00,4";
const char *RecordPort::SESSION_IS_CREATED = "G,E00,5";
const char *RecordPort::ERROR_INI_SESSION = "G,E00,6"; 
const char *RecordPort::RECORD_SRV_REINI = "G,T11";	//Mensaje enviado por el servicio de grabacion por el 65001

int RecordPort::timer_id = 1;

/**
 * RecordPort.	...
 * Crea el puerto pjmedia de grabación.
 * @param	TerminalIpAdd		Direccion IP del Terminal/Pasarela. Es la que se envía con el comando de Notificacion direccion IP Pasarela
 * @param	RecIp				Direccion IP del grabador.
 * @param	recPort				Puerto IP del grabador.
 * @param	RecursoTipoTerminal		Cadena de caracteres con el identificacor del Recurso Tipo Terminal
 * @return	nada.
 */
RecordPort::RecordPort(int resType, const char * TerminalIpAdd, const char * RecIp, unsigned recPort, const char *TerminalId)
{
	pj_memset(this, 0, sizeof(RecordPort));	

	_Pool = NULL;
	_Sock = PJ_INVALID_SOCKET;
	_SockSt = PJ_INVALID_SOCKET;
	_RemoteSock = NULL;
	_RemoteStSock = NULL;
	_Lock = NULL;
	TheOtherRec = NULL;
	Slot = PJSUA_INVALID_ID;
	ctrlSessEvent = NULL;
	actions_thread = NULL;
	actions_sem = NULL;
	session_thread = NULL;
	pj_bzero(_RecursoTipoTerminal, sizeof(_RecursoTipoTerminal));
	nsec_media = 0;
	SessStatus = RECPORT_SESSION_IDLE;
	Resource_type = resType;
	pj_bzero(&t_last_command, sizeof(t_last_command));
	recording_by_rad = 0;
	recording_by_tel = 0;
	memset(frequencies, 0, sizeof(frequencies));
	/*Se inicializa el array SlotsToSndPorts*/
	for (int i = 0; i < CORESIP_MAX_SLOTSTOSNDPORTS; i++)
	{
		SlotsToSndPorts[i] = PJSUA_INVALID_ID;
	}
	
	SES_TIM_ID = ++timer_id;
	COM_TIM_ID = ++timer_id;
	
#ifdef REC_IN_FILE
	sim_rec_fd = NULL;
	sim_rec_tx_fd = NULL;
#endif

	Wait_response_timeout_sec = 3;		
	pj_timer_entry_init( &send_command_timer, 0, NULL, send_command_timer_cb);

	try
	{	
		strcpy(RecTerminalIpAdd, TerminalIpAdd); 

		_Pool = pjsua_pool_create(NULL, 1024, 512);

		pj_assert(sizeof(_RecursoTipoTerminal) > strlen(TerminalId));

		strcpy(_RecursoTipoTerminal, TerminalId);
		if (resType == TEL_RESOURCE)
			strcat(_RecursoTipoTerminal, "-TEL");
		else
			strcat(_RecursoTipoTerminal, "-RAD");

		pj_status_t st = pj_lock_create_recursive_mutex(_Pool, NULL, &_Lock);
		PJ_CHECK_STATUS(st, ("ERROR creando seccion critica para puerto de grabacion RecordPort"));

		pjmedia_port_info_init(&_Port.info, &(pj_str("RECP")), PJMEDIA_PORT_SIGNATURE('R', 'E', 'C', 'P'), 
				SAMPLING_RATE, CHANNEL_COUNT, BITS_PER_SAMPLE, SAMPLES_PER_FRAME);

		_Port.port_data.pdata = this;
		_Port.put_frame = &PutFrame;     //Se llama cuando el puerto src hace put_frame hacia este puerto
		_Port.reset = &Reset;

		if (resType == TEL_RESOURCE)
		{
			//Se crea el socket para comunicacion con el servicio de grabacion por el puerto estatico ST_PORT
			//Se crea solamente en el recurso del tipo telefonia
			pj_sockaddr_in addStIpToBind;
			pj_bzero(&addStIpToBind, sizeof(addStIpToBind));
			addStIpToBind.sin_family = pj_AF_INET();
			addStIpToBind.sin_port = pj_htons(ST_PORT);

			st = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &_SockSt);
			PJ_CHECK_STATUS(st, ("ERROR creando socket _SockSt para envio al grabador"));

			st = pj_sock_bind(_SockSt, &addStIpToBind, sizeof(addStIpToBind));
			PJ_CHECK_STATUS(st, ("ERROR enlazando socket para puerto de grabacion", "[Ip=%s][Port=%d]", "0.0.0.0", ST_PORT));
		}

		st = pj_mutex_create_simple(_Pool, "frequencies_mutex", &frequencies_mutex);
		PJ_CHECK_STATUS(st, ("ERROR creando frequencies_mutex del puerto del grabador"));

		//Se crea el socket para comunicacion con el servicio de grabacion
		pj_sockaddr_in addrIpToBind;
		pj_bzero(&addrIpToBind, sizeof(addrIpToBind));
		addrIpToBind.sin_family = pj_AF_INET();
		addrIpToBind.sin_port = pj_htons(0);

		st = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &_Sock);
		PJ_CHECK_STATUS(st, ("ERROR creando socket para envio al grabador"));

		st = pj_sock_bind(_Sock, &addrIpToBind, sizeof(addrIpToBind));
		PJ_CHECK_STATUS(st, ("ERROR enlazando socket para puerto de grabacion", "[Ip=%s][Port=%d]", "0.0.0.0", 0));

		//Thread ejecucion acciones sobre grabador
		_Pool = pjsua_pool_create(NULL, 1024, 512);

		st = pj_mutex_create_simple(_Pool, "RecActionsMtx", &record_mutex);
		PJ_CHECK_STATUS(st, ("ERROR creando mutex de conexion puerto audio con grabador"));

		st = pj_sem_create(_Pool, "RecActionsSem", 0, MAX_REC_COMMANDS_QUEUE*2, &actions_sem);
		PJ_CHECK_STATUS(st, ("ERROR creando semaforo de acciones al grabador"));		

		st = pj_mutex_create_simple(_Pool, "RecActionsMtx", &actions_mutex);
		PJ_CHECK_STATUS(st, ("ERROR creando mutex de acciones sobre el grabador"));

		Rec_Command_queue.iRecw = 0;
		Rec_Command_queue.iRecr = 0;
		Rec_Command_queue.RecComHoles = MAX_REC_COMMANDS_QUEUE;

		Rec_RecPau_queue.iRecw = 0;
		Rec_RecPau_queue.iRecr = 0;
		Rec_RecPau_queue.RecComHoles = MAX_REC_COMMANDS_QUEUE;

		actions_thread_run = 1;
		st = pj_thread_create(_Pool, "RecordActionsTh", &RecordActionsTh, this, 0, 0, &actions_thread);
		PJ_CHECK_STATUS(st, ("ERROR creando thread de lectura del grabador"));

		//Thread lectura mensajes del grabador
		_Pool = pjsua_pool_create(NULL, 1024, 512);
		st = pj_sem_create(_Pool, "ReadServiceSem", 0, 1, &sem);
		PJ_CHECK_STATUS(st, ("ERROR creando semaforo de lectura del grabador"));

		st = pj_mutex_create_simple(_Pool, "ReadServiceMtx", &mutex);
		PJ_CHECK_STATUS(st, ("ERROR creando mutex del puerto del grabador"));

		pj_activesock_cb cb = { NULL };
		cb.on_data_recvfrom = &OnDataReceived;

		st = pj_activesock_create(_Pool, _Sock, pj_SOCK_DGRAM(), NULL, pjsip_endpt_get_ioqueue(pjsua_get_pjsip_endpt()), &cb, this, &_RemoteSock);
		PJ_CHECK_STATUS(st, ("ERROR creando servidor de lectura respuestas del grabador"));
		
		st = pj_activesock_start_recvfrom(_RemoteSock, _Pool, MAX_MSG_LEN, 0);
		PJ_CHECK_STATUS(st, ("ERROR iniciando lectura respuestas del grabador"));

		if (resType == TEL_RESOURCE)
		{
			//Se crea el active socket para comunicacion con el servicio de grabacion por el puerto estatico 65001
			//Se crea solamente en el recurso del tipo telefonia
			pj_activesock_cb st_cb = { NULL };
			st_cb.on_data_recvfrom = &OnDataReceivedSt;

			st = pj_activesock_create(_Pool, _SockSt, pj_SOCK_DGRAM(), NULL, pjsip_endpt_get_ioqueue(pjsua_get_pjsip_endpt()), &st_cb, this, &_RemoteStSock);
			PJ_CHECK_STATUS(st, ("ERROR creando servidor de lectura respuestas del grabador"));
		
			st = pj_activesock_start_recvfrom(_RemoteStSock, _Pool, MAX_MSG_LEN, 0);
			PJ_CHECK_STATUS(st, ("ERROR iniciando lectura respuestas del grabador"));
		}		

		//Thread de control de la session
		st = pj_event_create(_Pool, "CtrlSessEvent", false, false, &ctrlSessEvent);
		PJ_CHECK_STATUS(st, ("ERROR creando CtrlSessEvent"));

		sessionControlTh_run = 1;
		st = pj_thread_create(_Pool, "SessionControl", &SessionControlTh, this, 0, 0, &session_thread);
		PJ_CHECK_STATUS(st, ("ERROR creando thread de control de sesion del grabador"));
				
		pj_sockaddr_in_init(&recAddr, &(pj_str(const_cast<char*>(RecIp))), (pj_uint16_t)recPort);
		
		//Configura timer para actualizar estado de la sesion de grabacion		
		st = StartSessionTimer(NO_SESSION_TIMER);
		if (st != PJ_SUCCESS)
		{
			update_session_timer.id = 0;
		}
		PJ_CHECK_STATUS(st, ("ERROR Creando Timer de control de sesion"));

		pj_event_set(ctrlSessEvent);  //Activamos el evento para que en la tarea SessionControlTh se envie 
										//la IP y se inicie sesion
		
#ifdef REC_IN_FILE
		if (Resource_type == TEL_RESOURCE)
		{
			st = pj_file_open (_Pool, "simu_record.bin", PJ_O_WRONLY /*| PJ_O_APPEND*/, &sim_rec_fd);
			PJ_CHECK_STATUS(st, ("ERROR simu_record.bin no se puede abrir"));
		}
		else
		{
			st = pj_file_open (_Pool, "simu_record_rad.bin", PJ_O_WRONLY /*| PJ_O_APPEND*/, &sim_rec_fd);
			PJ_CHECK_STATUS(st, ("ERROR simu_record_rad.bin no se puede abrir"));
		}
		st = pj_file_open (_Pool, "simu_record.txt", PJ_O_WRONLY /*| PJ_O_APPEND*/, &sim_rec_tx_fd);
		PJ_CHECK_STATUS(st, ("ERROR simu_record.txt no se puede abrir"));
#endif

		st = pjsua_conf_add_port(_Pool, &_Port, &Slot);
		PJ_CHECK_STATUS(st, ("ERROR enlazando RecordPort al mezclador"));
	}
	catch (...)
	{
		Dispose();
		throw;
	}
}

void RecordPort::Dispose()
{
	pj_status_t st = PJ_SUCCESS;

	PJ_LOG(3,(__FILE__, "DISPOSE %s", _RecursoTipoTerminal));	

	Wait_response_timeout_sec = 1;
	RecRemoveRecObj();
	
#ifdef REC_IN_FILE
	if (sim_rec_fd != NULL)
		pj_file_close(sim_rec_fd);
	if (sim_rec_tx_fd != NULL)
		pj_file_close(sim_rec_tx_fd);
#endif		

	if (actions_thread != NULL && actions_sem != NULL)
	{
		actions_thread_run = 0;
		Wait_response_timeout_sec = 0;
		pj_sem_post(actions_sem);
		st = pj_thread_join(actions_thread);
		st = pj_thread_destroy(actions_thread);	
	}
	
	if (session_thread != NULL && ctrlSessEvent != NULL)
	{
		sessionControlTh_run = 0;
		pj_event_set(ctrlSessEvent);
		st = pj_thread_join(session_thread);
		st = pj_thread_destroy(session_thread);	
	}	

	update_session_timer.id = 0;
	pjsua_cancel_timer(&update_session_timer);

	send_command_timer.id = 0;
	pjsua_cancel_timer(&send_command_timer);

	recording_by_rad = 0;
	SipAgent::RecConnectSndPorts(false, this);
	ConnectRx(false);

	if (Slot != PJSUA_INVALID_ID)
	{
		pjsua_conf_remove_port(Slot);
	}	

	if (_RemoteSock != NULL)
	{
		pj_activesock_close(_RemoteSock);
	}

	if (_Sock != PJ_INVALID_SOCKET)
	{
		pj_sock_close(_Sock);
	}

	if (_RemoteStSock != NULL)
	{
		pj_activesock_close(_RemoteStSock);
	}

	if (_SockSt != PJ_INVALID_SOCKET)
	{
		pj_sock_close(_SockSt);
	}

	if (_Pool)
	{
		pj_pool_release(_Pool);
	}
}

RecordPort::~RecordPort(void)
{
	Dispose();
}

void RecordPort::SetTheOtherRec(RecordPort *TheOtherRec_)
{
	if (TheOtherRec_ != this)
	{
		TheOtherRec = TheOtherRec_;
	}
}

/**
 * SessionControlTh.	...
 * Tarea que controla el estado de la sesion de grabacion.
 * @return	retorno de la tarea.
 */
int RecordPort::SessionControlTh(void *proc)
{
	//Tarea para actualizar la sesion de grabacion
	RecordPort *wp = (RecordPort *)proc;

	pj_thread_desc desc;
    pj_thread_t *this_thread;
	pj_status_t rc;

	pj_bzero(desc, sizeof(desc));

    rc = pj_thread_register("SessionControlTh", desc, &this_thread);
    if (rc != PJ_SUCCESS) {
		PJ_LOG(3,(__FILE__, "...error in pj_thread_register SessionControlTh!"));
        return 0;
    }

    /* Test that pj_thread_this() works */
    this_thread = pj_thread_this();
    if (this_thread == NULL) {
        PJ_LOG(3,(__FILE__, "...error: SessionControlTh pj_thread_this() returns NULL!"));
        return 0;
    }

    /* Test that pj_thread_get_name() works */
    if (pj_thread_get_name(this_thread) == NULL) {
        PJ_LOG(3,(__FILE__, "...error: SessionControlTh pj_thread_get_name() returns NULL!"));
        return 0;
    }

	while (wp->sessionControlTh_run)
	{
		pj_event_wait(wp->ctrlSessEvent);		
		if (!wp->sessionControlTh_run)
			break;
		wp->CancelSessionTimer();
		wp->SessStatus = RECPORT_SESSION_IDLE;
		pj_thread_sleep(1000);

		//Limpio el evento para que no se embucle este hilo
		while (pj_event_trywait(wp->ctrlSessEvent) == PJ_SUCCESS);		

		//ha habido un error, como por ejemplo que el grabador se ha reseteado
		//Se inicia la sesion desde cero
		pj_mutex_lock(wp->actions_mutex);
		while (pj_sem_trywait(wp->actions_sem) == PJ_SUCCESS); 
		wp->Rec_Command_queue.iRecw = 0;
		wp->Rec_Command_queue.iRecr = 0;
		wp->Rec_Command_queue.RecComHoles = MAX_REC_COMMANDS_QUEUE;
		wp->Rec_RecPau_queue.iRecw = 0;
		wp->Rec_RecPau_queue.iRecr = 0;
		wp->Rec_RecPau_queue.RecComHoles = MAX_REC_COMMANDS_QUEUE;
		pj_mutex_unlock(wp->actions_mutex);			

		wp->recording_by_rad = 0;
		wp->recording_by_tel = 0;

		pj_mutex_lock(wp->frequencies_mutex);
		memset(wp->frequencies, 0, sizeof(wp->frequencies));
		pj_mutex_unlock(wp->frequencies_mutex);
		
		wp->StartSessionTimer(NO_SESSION_TIMER);		

		wp->SessStatus = RECPORT_SESSION_IDLE;
		int ret = 0;
		
		PJ_LOG(5,(__FILE__, "TRAZA: RECORD wp->NotifIp %s", wp->_RecursoTipoTerminal));	
		ret = wp->NotifIp(true);	
		if (ret != 0)
		{
			PJ_LOG(5,(__FILE__, "TRAZA: RECORD wp->RecRemoveRecObj 0 %s", wp->_RecursoTipoTerminal));
			ret = wp->RecRemoveRecObj();
			pj_event_set(wp->ctrlSessEvent);
			continue;
		}

		PJ_LOG(5,(__FILE__, "TRAZA: RECORD wp->RecSession false %s", wp->_RecursoTipoTerminal));	
		ret = wp->RecSession(false, true);
		if (ret != 0)
		{
			PJ_LOG(5,(__FILE__, "TRAZA: RECORD wp->RecRemoveRecObj 1 %s", wp->_RecursoTipoTerminal));
			ret = wp->RecRemoveRecObj();
		}		
		if (ret == 0)
		{
			PJ_LOG(5,(__FILE__, "TRAZA: RECORD wp->RecSession true %s", wp->_RecursoTipoTerminal));	
			ret = wp->RecSession(true, true);
			if (ret != 0)
			{
				PJ_LOG(5,(__FILE__, "TRAZA: RECORD wp->RecRemoveRecObj 2 %s", wp->_RecursoTipoTerminal));	
				wp->RecRemoveRecObj();
			}
		}
		if (ret != 0)
		{			
			PJ_LOG(5,(__FILE__, "TRAZA: RECORD wp->ctrlSessEvent %s", wp->_RecursoTipoTerminal));	
			pj_event_set(wp->ctrlSessEvent);
		}
		else
		{
			//Limpio el evento para que no se embucle este hilo
			while (pj_event_trywait(wp->ctrlSessEvent) == PJ_SUCCESS);
		}

		pj_thread_sleep(20);
	}

	return 0;
}

/**
 * RecordActionsTh.	...
 * Tarea que envía los comandos de grabación y espera la respuesta.
 * @return	retorno de la tarea.
 */
int RecordPort::RecordActionsTh(void *proc)
{
	RecordPort *wp = (RecordPort *)proc;
	pj_status_t st = PJ_SUCCESS;

	pj_thread_desc desc;
    pj_thread_t *this_thread;
	pj_status_t rc;

	pj_bzero(desc, sizeof(desc));

    rc = pj_thread_register("RecordActionsTh", desc, &this_thread);
    if (rc != PJ_SUCCESS) {
		PJ_LOG(3,(__FILE__, "...error in pj_thread_register RecordActionsTh!"));
        return 0;
    }

    /* Test that pj_thread_this() works */
    this_thread = pj_thread_this();
    if (this_thread == NULL) {
        PJ_LOG(3,(__FILE__, "...error: RecordActionsTh pj_thread_this() returns NULL!"));
        return 0;
    }

    /* Test that pj_thread_get_name() works */
    if (pj_thread_get_name(this_thread) == NULL) {
        PJ_LOG(3,(__FILE__, "...error: RecordActionsTh pj_thread_get_name() returns NULL!"));
        return 0;
    }

	//Mientras haya mensajes a enviar no termina la tarea
	while (wp->actions_thread_run)
	{
		st = pj_sem_wait(wp->actions_sem);
		if (st != PJ_SUCCESS) break;
		//PJ_CHECK_STATUS(st, ("ERROR pj_sem_wait(wp->actions_sem)"));
		if (!wp->actions_thread_run) 
		{
			continue;
		}

		char mess[MAX_COMMAND_LEN];

		st = pj_mutex_lock(wp->actions_mutex);
		if (st != PJ_SUCCESS)
		{
			break;
			//PJ_CHECK_STATUS(st, ("ERROR pj_mutex_lock(wp->actions_mutex)"));
		}		

		COMMAND_QUEUE *Command_queue;

		if (wp->Rec_RecPau_queue.RecComHoles < MAX_REC_COMMANDS_QUEUE)
		{
			//Hay mensajes RECORD/PAUSE en cola. Tienen prioridad
			Command_queue = &wp->Rec_RecPau_queue;			
		}
		else if (wp->Rec_Command_queue.RecComHoles < MAX_REC_COMMANDS_QUEUE)
		{
			Command_queue = &wp->Rec_Command_queue;
		}
		else
		{
			st = pj_mutex_unlock(wp->actions_mutex);
			if (st != PJ_SUCCESS) break;
			//PJ_CHECK_STATUS(st, ("ERROR pj_mutex_unlock en RecordActionsTh"));
			continue;
		}

		pj_memcpy(mess, Command_queue->queue[Command_queue->iRecr].buf, 
			Command_queue->queue[Command_queue->iRecr].len);
		pj_ssize_t messlen = Command_queue->queue[Command_queue->iRecr].len;
		if (++Command_queue->iRecr == MAX_REC_COMMANDS_QUEUE)
			Command_queue->iRecr = 0;
		Command_queue->RecComHoles++;

		st = pj_mutex_unlock(wp->actions_mutex);
		if (st != PJ_SUCCESS) break;
		//PJ_CHECK_STATUS(st, ("ERROR pj_mutex_unlock en RecordActionsTh"));

		REC_RESPONSE res;	

		if (strncmp(mess, REC_RECORD, strlen(REC_RECORD)) == 0)
		{			
			if (wp->Resource_type == TEL_RESOURCE)
			{
				if (wp->recording_by_tel > 0)
				{
					continue;
				}
				else
				{
					int numcalls = SipAgent::NumConfirmedCalls();
					if (numcalls > 0)
					{
						wp->recording_by_tel = 1;
					}
					else
					{
						//No enviamos la orden de record si no hay llamadas confimadas con media
						continue;
					}
				}
			}
			else
			{
				if (wp->recording_by_rad > 0)
				{
					//Existen otros recursos que ya están grabando.
					//No se envía el comando
					continue;
				}
				wp->recording_by_rad = 1;
			}
		}
		else if (strncmp(mess, REC_PAUSE, strlen(REC_PAUSE)) == 0)
		{
			if (wp->Resource_type == TEL_RESOURCE)
			{
				//Si el mensaje es PAUSE comprobamos si podemos enviarlo o no
				//dependiendo del número de llamadas con media
				int numcalls = SipAgent::NumConfirmedCalls();
				if (numcalls > 0 || wp->recording_by_tel == 0)
				{
					continue;
				}

				wp->recording_by_tel = 0;
			}
			else if (wp->Resource_type == RAD_RESOURCE)
			{	
				int recording_by_ptt_aux;
				int recording_by_squ_aux;
				int recording_by_rad_aux = wp->recording_by_rad;

				wp->GetNumSquPtt(&recording_by_ptt_aux, &recording_by_squ_aux);
				wp->recording_by_rad = recording_by_ptt_aux + recording_by_squ_aux;

				if (wp->recording_by_rad > 0)
					//Si hay alguna frecuencia con squelch o ptt activado entonces no mandamos el PAUSE
					continue;
				else if (recording_by_rad_aux == 0)
				{
					//No hay cambio de estado. No se envía PAUSE. Ya se ha enviado.
					continue;
				}
			}
			
			//Se desconectan todos los puertos que estaban conectados con el de grabacion tanto Tx como Rx
			SipAgent::RecConnectSndPorts(false, wp);
			wp->ConnectRx(false);
		}
		else if (strncmp(mess, REC_HOLDON, strlen(REC_HOLDON)) == 0)
		{
			/*Se ha visto en el grabador de Tenerife que con el comando de aparcar 
				la grabación se interrumpe. Por lo tanto si hay más de 1 llamada confirmada 
				entonces no lo enviamos porque si no se cortaría la grabacion de la voz de la otra llamada. Esto
				se ha detectado en las transferencias*/
			int numcalls = SipAgent::NumConfirmedCalls();
			if (numcalls > 1)
			{
				continue;
			}
		}
		else if (strncmp(mess, REC_PTTON, strlen(REC_PTTON)) == 0)
		{			
		}
		else if (strncmp(mess, SQUELCH_ON, strlen(SQUELCH_ON)) == 0)
		{
		}
		else if (strncmp(mess, REC_PTTOFF, strlen(REC_PTTOFF)) == 0)
		{

		}
		else if (strncmp(mess, SQUELCH_OFF, strlen(SQUELCH_OFF)) == 0)
		{

		}

		pj_thread_sleep(SLEEP_FOR_SIMU);

		res = wp->SendCommand(mess, (pj_ssize_t *) &messlen, &wp->recAddr, sizeof(wp->recAddr), true);

		bool update_sesion = false;

		if (res == REC_NO_RESPONSE)
		{
			update_sesion = true;
			PJ_LOG(3,(__FILE__, "ERROR: Recorder does not respond. message sent %s", mess));			
		}
		else if ((strncmp(mess, FIN_SES_REC_TERM, strlen(FIN_SES_REC_TERM)) == 0 && wp->Resource_type == TEL_RESOURCE) ||
			(strncmp(mess, FIN_SES_REC_RAD, strlen(FIN_SES_REC_RAD)) == 0 && wp->Resource_type == RAD_RESOURCE))
		{
			if (res == REC_OK_RESPONSE || res == REC_SESSION_CLOSED)
			{
				wp->SessStatus = RECPORT_SESSION_CLOSED;
			}
			else 
			{
				wp->SessStatus = RECPORT_SESSION_ERROR;
				PJ_LOG(3,(__FILE__, "ERROR: Record Session cannot be finished"));
				update_sesion = true;
			}
		}
		else if ((strncmp(mess, INI_SES_REC_TERM, strlen(INI_SES_REC_TERM)) == 0 && wp->Resource_type == TEL_RESOURCE) ||
			(strncmp(mess, INI_SES_REC_RAD, strlen(INI_SES_REC_RAD)) == 0 && wp->Resource_type == RAD_RESOURCE))
		{
			if (res == REC_OK_RESPONSE || res == REC_SESSION_IS_ALREADY_CREATED) wp->SessStatus = RECPORT_SESSION_OPEN;
			else 
			{
				wp->SessStatus = RECPORT_SESSION_ERROR;
				PJ_LOG(3,(__FILE__, "ERROR: Record Session cannot be started"));
				update_sesion = true;
			}
		}
		else if (strncmp(mess, NOTIF_IPADD, strlen(NOTIF_IPADD)) == 0)
		{
			if (res == REC_OK_RESPONSE)
			{
				wp->SessStatus = RECPORT_SESSION_IPSEND;
			}
			else
			{
				wp->SessStatus = RECPORT_SESSION_ERROR;
				PJ_LOG(3,(__FILE__, "ERROR: Sending NOTIF_IPADD"));
			}
		}
		else if (strncmp(mess, REMOVE_REC_OBJ, strlen(REMOVE_REC_OBJ)) == 0)
		{
			if (res == REC_OK_RESPONSE)
			{
				wp->SessStatus = RECPORT_SESSION_IDLE;
			}
			else
			{
				wp->SessStatus = RECPORT_SESSION_ERROR;
				PJ_LOG(3,(__FILE__, "ERROR: Sending REMOVING RECORD OBJECT"));
			}
		}
		else 
		{	
			if (res == REC_SESSION_CLOSED || res == REC_ERROR_INI_SESSION)
			{
				update_sesion = true;
			}
			else if (strncmp(mess, REC_RECORD, strlen(REC_RECORD)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending RECORD"));
					//Ha fallado el comando record. Ponemos la variable a 0 para que cuando se reciba media
					//en PutFrame vuelva a enviar el RECORD
					if (wp->Resource_type == TEL_RESOURCE)
						wp->recording_by_tel = 0;
					else if (wp->Resource_type == RAD_RESOURCE)
						wp->recording_by_rad = 0;
				}
				else
				{
					//Se conectan los puertos de sonido en RX. De llamadas o radios
					wp->ConnectRx(true);			
				}
			}
			else if (strncmp(mess, REC_PAUSE, strlen(REC_PAUSE)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending PAUSE"));
				}
				else
				{
								
				}
			}
			else if (strncmp(mess, REC_PTTON, strlen(REC_PTTON)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending PTT ON"));
				}
				else
				{
					//Se conectan los puertos de sonido en RX. De llamadas o radios
					//wp->ConnectRx(true);			
				}
			}
			else if (strncmp(mess, REC_PTTOFF, strlen(REC_PTTOFF)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending PTT OFF"));
				}
			}
			else if (strncmp(mess, SQUELCH_ON, strlen(SQUELCH_ON)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending SQUELCH ON"));
				}
				else
				{
					//Se conectan los puertos de sonido en RX. De llamadas o radios
					//wp->ConnectRx(true);
				}
			}
			else if (strncmp(mess, SQUELCH_OFF, strlen(SQUELCH_OFF)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending SQUELCH OFF"));
				}
			}
			else if (strncmp(mess, REC_CALLSTART, strlen(REC_CALLSTART)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending CallStart"));
				}
			}
			else if (strncmp(mess, REC_CALLEND, strlen(REC_CALLEND)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending CallEnd"));
				}
			}
			else if (strncmp(mess, REC_CALLCONNECTED, strlen(REC_CALLCONNECTED)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending CallConnected"));
				}
			}
			else if (strncmp(mess, REC_HOLDON, strlen(REC_HOLDON)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending HOLD ON"));
				}
			}
			else if (strncmp(mess, REC_HOLDOFF, strlen(REC_HOLDOFF)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending HOLD OFF"));
				}
			}
			else if (strncmp(mess, REC_RESET, strlen(REC_RESET)) == 0)
			{
				if (res != REC_OK_RESPONSE)
				{
					PJ_LOG(3,(__FILE__, "ERROR: Sending RECORDER RESET"));
				}
			}
		}

		if (update_sesion)
		{
			//Se fuerza la actualización del control de la sesion
			wp->RecResetSession();			
		}
		else
		{
			//El servicio de grabación ha contestado de forma correcta con lo cual
			//no hace refrescar el timer de cerrar y abrir sesion periódicamente.
			wp->CancelSessionTimer(); 
		}

		pj_gettimeofday(&wp->t_last_command);
	}

	return 0;
}

pj_bool_t RecordPort::OnDataReceived(pj_activesock_t * asock, void * data, pj_size_t size, const pj_sockaddr_t *src_addr, int addr_len, pj_status_t status)
{
	RecordPort * wp = (RecordPort*)pj_activesock_get_user_data(asock);
	pj_status_t st = PJ_SUCCESS;
	char mess[MAX_MSG_LEN+1];
	
	if (status == PJ_SUCCESS)
	{
		if (size != MAX_MSG_LEN)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Bad response from Record Service. Message is very long"));
			return PJ_TRUE;
		}

		memcpy(mess, data, size); 
		mess[size] = '\0';

		if (strcmp(OVRFLOW, mess) == 0)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Media Record: Overflow. Message Received: %s", mess));
		}
		else
		{
			PJ_LOG(5,(__FILE__, "TRAZA: RECORD: OnDataReceived Message Received: %s", mess));
			pj_mutex_lock(wp->mutex);
			strcpy(wp->message_received, mess);
			pj_sem_post(wp->sem);
			pj_mutex_unlock(wp->mutex);
		}
	}

	return PJ_TRUE;
}

pj_bool_t RecordPort::OnDataReceivedSt(pj_activesock_t * asock, void * data, pj_size_t size, const pj_sockaddr_t *src_addr, int addr_len, pj_status_t status)
{
	RecordPort * wp = (RecordPort*)pj_activesock_get_user_data(asock);
	pj_status_t st = PJ_SUCCESS;
	char mess[MAX_MSG_LEN+1];
	
	if (status == PJ_SUCCESS)
	{
		if (size > MAX_MSG_LEN)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Bad message from Record Service. Message is very long"));
			return PJ_TRUE;
		}

		memcpy(mess, data, size); 
		mess[size] = '\0';

		if (strcmp(RECORD_SRV_REINI, mess) == 0)
		{
			PJ_LOG(3,(__FILE__, "RecordPort: Recibido mensaje por el puerto estatico %u: %s. Se ha reiniciado el servicio de grabacion", ST_PORT, mess));	
			wp->RecResetSession();
			if (wp->TheOtherRec != NULL)
			{
				wp->TheOtherRec->RecResetSession();
			}
		}
		else
		{
			PJ_LOG(3,(__FILE__, "RecordPort: Recibido mensaje por el puerto estatico %u: %s. Mensaje no reconocido.", ST_PORT, mess));	
		}
	}

	return PJ_TRUE;
}

/**
 * RecResetSession.	...
 * Se reinicia la sesion de grabación
 * @return	< 0, si hay error.
 */
int RecordPort::RecResetSession()
{
	//Se fuerza la actualización del control de la sesion
	SessStatus = RECPORT_SESSION_ERROR;
	pj_event_set(ctrlSessEvent);
	pj_thread_sleep(20);
	return 0;
}

/**
 * SendCommand.	...
 * Envía comando al grabador y espera a recibir la respuesta
 * @param mess. Mensaje a enviar.
 * @param len. Longitud del mensaje
 * @param to. Dirección de destino.
 * @param tolen. size de to.
 * @return	REC_RESPONSE.
 */
REC_RESPONSE RecordPort::SendCommand(char *mess, pj_ssize_t *len, const pj_sockaddr_t *to, int tolen, pj_bool_t espera_respuesta)
{
	pj_status_t st = PJ_SUCCESS;
	int ntries = TRIES_SENDING_CMD;
	REC_RESPONSE ret = REC_NO_RESPONSE;

	mess[*len] = 0;

#ifdef DEBUG_GRABACION
	PJ_LOG(3,(__FILE__, "DEBUG: GRAB137 %s", mess));	
#endif

	PJ_LOG(5,(__FILE__, "RecordPort: Sending message %s", mess));

#ifdef REC_IN_FILE
	pj_ssize_t mess_len_tx;
	mess_len_tx = *len;
	pj_file_write(sim_rec_tx_fd, mess, (pj_ssize_t *) &mess_len_tx);
	mess_len_tx = 1;
	pj_file_write(sim_rec_tx_fd, "\n", (pj_ssize_t *) &mess_len_tx);
#endif
		
	do {
		st = pj_sock_sendto(_Sock, mess, (pj_ssize_t *) len, 0, to, tolen);
		if (st != PJ_SUCCESS)
		{
			char buf[256];
			pj_strerror(st, buf, sizeof(buf));
			PJ_LOG(5,(__FILE__, "ERROR: pj_sock_sendto ERROR SOCK %s", buf));
		}
		else if (!espera_respuesta)
		{
			break;
		}
		else
		{
			ret = ReadResp();
			if (ret != REC_NO_RESPONSE && ret != REC_BAD_RESPONSE)
				break;
		}
	} while (--ntries > 0);		

	return ret;
}

/**
 * ReadResp.	...
 * Procesa la respuesta del grabador
 * @return	REC_RESPONSE.
 */
REC_RESPONSE RecordPort::ReadResp()
{
	REC_RESPONSE ret = REC_OK_RESPONSE;
	pj_status_t st = PJ_SUCCESS;

#ifdef DEBUG_GRABACION
	return ret;
#endif

	//Esperamos 3000ms (3 seg) a recibir respuesta del Servicio de Grabacion
	//Configura timer de espera de las respuestas de los mensajes al grabador
	send_command_timer.id = 0;
	pjsua_cancel_timer(&send_command_timer);
	send_command_timer.id = COM_TIM_ID;
	send_command_timer.cb = send_command_timer_cb;
	send_command_timer.user_data = (void *) this;
	pj_time_val	delay;
	delay.sec = Wait_response_timeout_sec;
	delay.msec = 0;
	pjsua_schedule_timer(&send_command_timer, &delay);

	st = pj_sem_wait(sem);
	send_command_timer.id = 0;
	pjsua_cancel_timer(&send_command_timer); 
	
	if (st != PJ_SUCCESS)
	{
		return REC_NO_RESPONSE;
	}

	pj_mutex_lock(mutex);
	if (strcmp(REC_NOT_MESSAGE, message_received) == 0) 
	{
		PJ_LOG(3,(__FILE__, "ERROR: Record Service does not respond"));
		ret = REC_NO_RESPONSE;
	}
	else if (strcmp(RESPOK, message_received) == 0) 
	{
		PJ_LOG(5,(__FILE__, "Message Received from recorder: %s", message_received));
		ret = REC_OK_RESPONSE;
	}
	else if (strcmp(BADDIRIP, message_received) == 0)
	{		
		ret = REC_IP_INCORRECT;
		PJ_LOG(3,(__FILE__, "ERROR: IP addres is not set to Record Service. Message Received: %s", message_received));
	}
	else if (strcmp(NOT_ACTIVE_SESSION, message_received) == 0)
	{
		ret = REC_SESSION_CLOSED;
		PJ_LOG(3,(__FILE__, "ERROR: There is not active record session. Message Received: %s", message_received));
	}
	else if (strcmp(OVRFLOW, message_received) == 0)
	{
		ret = REC_OVERFLOW;
		PJ_LOG(3,(__FILE__, "ERROR: Media Record: Overflow. Message Received: %s", message_received));
	}	
	else if (strcmp(SESSION_IS_CREATED, message_received) == 0)
	{
		ret = REC_SESSION_IS_ALREADY_CREATED;
		PJ_LOG(5,(__FILE__, "ERROR: Session is already created: %s", message_received));
	}
	else if (strcmp(ERROR_INI_SESSION, message_received) == 0)
	{
		ret = REC_ERROR_INI_SESSION;
		PJ_LOG(3,(__FILE__, "ERROR: Record Session cannot be started. Message Received: %s", message_received));
	}
	else if (strcmp(COMMAND_ERROR, message_received) == 0)
	{
		ret = REC_COMMAND_NO_SUPPORTED;
		PJ_LOG(3,(__FILE__, "ERROR: Command not supported. Message Received: %s", message_received));
	}
	else
	{
		ret = REC_BAD_RESPONSE;
		PJ_LOG(3,(__FILE__, "ERROR: Unknown. Message Received from Record Service: %s", message_received));
	}

	pj_mutex_unlock(mutex);
	return ret;
}

/**
 * Add_Rec_Command_Queue.	...
 * Añade un mensaje a la cola de comandos de grabación. 
 * @param	mess		mensaje
 * @param	messlen		longitud
 * @return	0 OK, -1  error.
 */
int RecordPort::Add_Rec_Command_Queue(char *mess, size_t messlen, COMMAND_QUEUE *Command_queue)
{
	int ret = 0;
	pj_status_t st = PJ_SUCCESS;

	st = pj_mutex_lock(actions_mutex);
	PJ_CHECK_STATUS(st, ("ERROR pj_mutex_lock in Add_Red_Command_Queue"));	

	if (Command_queue->RecComHoles > 0)
	{
		pj_memcpy(Command_queue->queue[Command_queue->iRecw].buf, mess, messlen);
		Command_queue->queue[Command_queue->iRecw].len = messlen;
		if (++Command_queue->iRecw == MAX_REC_COMMANDS_QUEUE)
			Command_queue->iRecw = 0;
		Command_queue->RecComHoles--;
		st = pj_sem_post(actions_sem);
		PJ_CHECK_STATUS(st, ("ERROR pj_sem_post in Add_Red_Command_Queue"));
	}
	else
	{
		PJ_LOG(3,(__FILE__, "ERROR: Record command queue is full. Recording service is reset"));
		ret = -1;
	}

	st = pj_mutex_unlock(actions_mutex);
	PJ_CHECK_STATUS(st, ("ERROR pj_mutex_unlock in Add_Red_Command_Queue"));

	return ret;
}

/**
 * NofifIp.	...
 * Envia el comando de notificación IP de la pasarela.
 * @param	wait_end	true - espera ejecucion comando / false - no espera
 * @return	0 OK, -1  error.
 */
int RecordPort::NotifIp(bool wait_end)
{
	int st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];
	
	int len_mess;
	pj_bzero(mess, sizeof(mess));

	strcpy(mess, NOTIF_IPADD);
	len_mess = strlen(mess)+strlen(RecTerminalIpAdd);		

	if (len_mess >= sizeof(mess)) 
	{
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{		
		strcat(mess, RecTerminalIpAdd);
		ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending NotifIp"));	
		}
	}

	if (ret == 0 && wait_end)
	{
		int sleep_ms = 20;
		int tries = (Wait_response_timeout_sec * 1000 * TRIES_SENDING_CMD) / sleep_ms;
		while (--tries > 0 && sessionControlTh_run && SessStatus != RECPORT_SESSION_IPSEND && SessStatus == RECPORT_SESSION_ERROR)
		{
			pj_thread_sleep(sleep_ms);
		}
		if (SessStatus == RECPORT_SESSION_ERROR || tries == 0)
		{
			PJ_LOG(5,(__FILE__, "ERROR: Recorder does not repond. Message sent %s", mess));
			ret = -1;
		}
	}

	PJ_CHECK_STATUS(st, ("ERROR al enviar mensaje Notificacion IP del terminal %s", _RecursoTipoTerminal));

	return ret;
}

/**
 * RecSession.	...
 * Abre o cierra sesion de grabación recurso Terminal.
 * @param	on			true - abre / false - cierra
 * @param	wait_end	true - espera ejecucion comando / false - no espera
 * @return	0 OK, -1  error.
 */
int RecordPort::RecSession(bool on, bool wait_end)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	//Envia mensaje de inicio/fin de sessión de grabación recurso terminal
	pj_bzero(mess, sizeof(mess));
	if (on)
	{
		if (Resource_type == RAD_RESOURCE) strcpy(mess, INI_SES_REC_RAD);
		else strcpy(mess, INI_SES_REC_TERM);
	}
	else
	{
		if (Resource_type == RAD_RESOURCE) strcpy(mess, FIN_SES_REC_RAD);
		else strcpy(mess, FIN_SES_REC_TERM);
	}

	if ((strlen(mess) + strlen(_RecursoTipoTerminal) + 1) > sizeof(mess))
	{
		//La cadena no cabe en mess
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		strcat(mess, _RecursoTipoTerminal);
		size_t messlen = strlen((const char *) mess);

		ret = Add_Rec_Command_Queue(mess, messlen, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecSession"));	
		}

		if (ret == 0 && wait_end)
		{
			int sleep_ms = 20;
			int tries = (Wait_response_timeout_sec * 1000 * TRIES_SENDING_CMD) / sleep_ms;
			if (on)
			{				
				while (--tries > 0 && sessionControlTh_run && SessStatus != RECPORT_SESSION_OPEN && SessStatus == RECPORT_SESSION_ERROR)
				{
					pj_thread_sleep(sleep_ms);
				}
			}
			else
			{
				while (--tries > 0 && sessionControlTh_run && SessStatus != RECPORT_SESSION_CLOSED && SessStatus == RECPORT_SESSION_ERROR)
				{
					pj_thread_sleep(sleep_ms);
				}
			}
			if (SessStatus == RECPORT_SESSION_ERROR || tries == 0)
			{
				PJ_LOG(5,(__FILE__, "ERROR: Recorder does not repond. Message sent %s", mess));
				ret = -1;
			}
		}
	}

	PJ_CHECK_STATUS(st, ("ERROR Abriendo o cerrando sesion de grabacion"));
	return ret;
}

/**
 * RecRemoveRecObj.	...
 * Elimina el objeto de grabación del recurso.
 * @return	0 OK, -1  error.
 */
int RecordPort::RecRemoveRecObj()
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	//Envia mensaje Elimina el objeto de grabación del recurso.
	pj_bzero(mess, sizeof(mess));	
	strcpy(mess, REMOVE_REC_OBJ);

	if ((strlen(mess) + strlen(_RecursoTipoTerminal) + 1) > sizeof(mess))
	{
		//La cadena no cabe en mess
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		strcat(mess, _RecursoTipoTerminal);
		size_t messlen = strlen((const char *) mess);

		ret = Add_Rec_Command_Queue(mess, messlen, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecRemoveRecObj"));	
		}		
	}

	if (ret == 0)
	{
		int sleep_ms = 20;
		int tries = (Wait_response_timeout_sec * 1000 * TRIES_SENDING_CMD) / sleep_ms;
		while (--tries > 0 && SessStatus != RECPORT_SESSION_IDLE
			&& SessStatus == RECPORT_SESSION_ERROR)
		{
			pj_thread_sleep(sleep_ms);
		}

		if (SessStatus == RECPORT_SESSION_ERROR || tries == 0)
		{
			PJ_LOG(5,(__FILE__, "ERROR: Recorder does not repond. Message sent %s", mess));
			ret = -1;
		}
	}

	PJ_CHECK_STATUS(st, ("ERROR Eliminando objeto de grabacion"));
	return ret;
}

/**
 * RecINV.	...
 * Envía mensaje INV.
 * @return	0 OK, -1  error.
 */
int RecordPort::RecINV()
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending RecINV. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}

	//Envia mensaje Elimina el objeto de grabación del recurso.
	pj_bzero(mess, sizeof(mess));	
	strcpy(mess, REC_INV);

	if ((strlen(mess) + strlen(_RecursoTipoTerminal) + 1) > sizeof(mess))
	{
		//La cadena no cabe en mess
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		strcat(mess, _RecursoTipoTerminal);
		size_t messlen = strlen((const char *) mess);

		ret = Add_Rec_Command_Queue(mess, messlen, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecRemoveRecObj"));	
		}		
	}
	
	PJ_CHECK_STATUS(st, ("ERROR Enviando INV"));
	return ret;
}

/**
 * RecBYE.	...
 * Envía mensaje BYE.
 * @return	0 OK, -1  error.
 */
int RecordPort::RecBYE()
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending RecBYE. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}

	//Envia mensaje Elimina el objeto de grabación del recurso.
	pj_bzero(mess, sizeof(mess));	
	strcpy(mess, REC_BYE);

	if ((strlen(mess) + strlen(_RecursoTipoTerminal) + 1) > sizeof(mess))
	{
		//La cadena no cabe en mess
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		strcat(mess, _RecursoTipoTerminal);
		size_t messlen = strlen((const char *) mess);

		ret = Add_Rec_Command_Queue(mess, messlen, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecRemoveRecObj"));	
		}		
	}
	
	PJ_CHECK_STATUS(st, ("ERROR Enviando INV"));
	return ret;
}

/**
 * RecReset.	...
 * Resetea el grabador
 * @return	0 OK, -1  error.
 */
int RecordPort::RecReset()
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[64];

	//Envia mensaje de reinicio del grabador.
	pj_bzero(mess, sizeof(mess));	
	strcpy(mess, REC_RESET);	
	size_t messlen = strlen((const char *) mess);
	ret = Add_Rec_Command_Queue(mess, messlen, &Rec_Command_queue);
	if (ret)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending RecReset"));	
	}		
	
	PJ_CHECK_STATUS(st, ("ERROR Enviando RESET AL GRABADOR"));
	return ret;
}

/**
 * RecCallStart.	...
 * Envía Inicio llamada telefonia al grabador
 * @param	dir			Direccion de la llamada 
 * @param	priority	Prioridad
 * @param	ori_uri		URI del llamante
 * @param	dest_uri	URI del llamado
 * @return	0 OK, -1  error.
 */
int RecordPort::RecCallStart(int dir, CORESIP_Priority priority, const pj_str_t *ori_uri, const pj_str_t *dest_uri)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];	

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending CallStart. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}	

	if (Resource_type != TEL_RESOURCE) return ret;

	char _tel[] = "tel:";
	char ori_tel[64];
	char dest_tel[64];

	GetTelNum(ori_uri->ptr, ori_uri->slen, ori_tel, sizeof(ori_tel));
	GetTelNum(dest_uri->ptr, dest_uri->slen, dest_tel, sizeof(dest_tel));

	pj_bzero(mess, sizeof(mess));
	strcpy(mess, REC_CALLSTART);	

	int len_mess = strlen(mess)+strlen(_RecursoTipoTerminal)+6+strlen(_tel)+strlen(ori_tel)+strlen(_tel)+strlen(dest_tel);
	if (len_mess >= sizeof(mess)) 
	{
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		strcat(mess, _RecursoTipoTerminal);
		strcat(mess, ",");

		switch(dir)
		{
		case INCOM:
			strcat(mess, "1");
			break;
		case OUTCOM:
			strcat(mess, "2");
			break;
		default:
			strcat(mess, "0");
		}
		strcat(mess, ",");
	
		switch(priority)
		{
		case CORESIP_PR_EMERGENCY:
			strcat(mess, "1");
			break;
		case CORESIP_PR_URGENT:
			strcat(mess, "2");
			break;
		case CORESIP_PR_NORMAL:
			strcat(mess, "3");
			break;
		case CORESIP_PR_NONURGENT:
			strcat(mess, "4");
			break;
		default:
			strcat(mess, "0");
		}
		strcat(mess, ",");

		strcat(mess, _tel);
		strcat(mess, ori_tel);
		strcat(mess, ",");
		strcat(mess, _tel);
		strcat(mess, dest_tel);

		ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecCallStart"));	
		}
	}
	PJ_CHECK_STATUS(st, ("ERROR al enviar mensaje CallStart", "(dir %d, priority %d)", dir, priority));

	return ret;
}

/**
 * RecCallEnd.	...
 * Envía Fin llamada telefonia al grabador
 * @param	cause		causa de desconexion
 * @param	disc_origin		origen de la desconexion
 * @return	0 OK, -1  error.
 */
int RecordPort::RecCallEnd(int cause, int disc_origin)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending CallEnd. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}

	if (Resource_type != TEL_RESOURCE) return ret;

	//Cortamos la grabacion al finalizar una llamada
	ret = Record(false);

	pj_bzero(mess, sizeof(mess));
	strcpy(mess, REC_CALLEND);

	char bufcause[64];
	pj_bzero(bufcause, sizeof(bufcause));
	pj_utoa((unsigned long)	cause, bufcause);	 

	char buforigin[64];
	pj_bzero(buforigin, sizeof(buforigin));
	pj_utoa((unsigned long)	disc_origin, buforigin);

	int len_mess = strlen(mess)+strlen(_RecursoTipoTerminal)+2+strlen(bufcause)+strlen(buforigin);
	if (len_mess >= sizeof(mess)) 
	{
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		strcat(mess, _RecursoTipoTerminal);
		strcat(mess, ",");
		strcat(mess, bufcause);
		strcat(mess, ",");
		strcat(mess, buforigin);

		ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecCallEnd"));	
		}
	}

	PJ_CHECK_STATUS(st, ("ERROR al enviar mensaje CallEnd", "(cause %d, disc_origin %d)", cause, disc_origin));

	return ret;
}

/**
 * RecCallConnected.	...
 * Envía llamada telefonia establecida al grabador
 * @param	cause		causa de desconexion
 * @param	disc_origin		origen de la desconexion
 * @return	0 OK, -1  error.
 */
int RecordPort::RecCallConnected(const pj_str_t *connected_uri)
{
	int st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending CallConnected. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}

	if (Resource_type != TEL_RESOURCE) return ret;

	char _tel[] = "tel:";
	char connected_tel[64];

	GetTelNum(connected_uri->ptr, connected_uri->slen, connected_tel, sizeof(connected_tel));

	pj_bzero(mess, sizeof(mess));
	strcpy(mess, REC_CALLCONNECTED);
	int len_mess = strlen(mess)+strlen(_RecursoTipoTerminal)+1+strlen(_tel)+strlen(connected_tel);
	if (len_mess >= sizeof(mess)) 
	{
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		strcat(mess, _RecursoTipoTerminal);
		strcat(mess, ",");
		strcat(mess, _tel);
		strcat(mess, connected_tel);

		ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecCallConnected %s", _RecursoTipoTerminal));	
		}
	}

	//Iniciamos la grabacion al conectarse la llamada
	ret = Record(true);

	PJ_CHECK_STATUS(st, ("ERROR al enviar mensaje CallConnected %s", _RecursoTipoTerminal));

	return ret;
}

/**
 * RecHold.	...
 * Envia evento Hold.
 * @param	on	true=ON, false=OFF
 * @param	llamante true=llamante false=llamado
 * @return	0 OK, -1  error.
 */
int RecordPort::RecHold(bool on, bool llamante)
{
	int st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending RecHold. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}

	if (Resource_type != TEL_RESOURCE) return ret;

	int len_mess;
	pj_bzero(mess, sizeof(mess));
	if (on) 
	{
		strcpy(mess, REC_HOLDON);
		len_mess = strlen(mess)+strlen(_RecursoTipoTerminal)+1+1;		
	}
	else 
	{
		strcpy(mess, REC_HOLDOFF);
		len_mess = strlen(mess)+strlen(_RecursoTipoTerminal);		
	}
	if (len_mess >= sizeof(mess)) 
	{
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (on && ret == 0)
	{
		ret = Record(false);
	}
		
	if (ret == 0)
	{
		strcat(mess, _RecursoTipoTerminal);
		strcat(mess, ",");
		if (on && llamante) 
			strncat(mess, "1", 1);
		else if (on && !llamante)
			strncat(mess, "2", 1);

		ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecCallConnected %s", _RecursoTipoTerminal));	
		}
	}
				
	if (!on && ret == 0)
	{
		ret = Record(true);
	}

	PJ_CHECK_STATUS(st, ("ERROR al enviar mensaje RecHold %s", _RecursoTipoTerminal));

	return ret;
}

/**
 * RecPTT.	...
 * Envía PTT ON al grabador para que se envíe el mensaje RECORD para recurso de radio
 * @param on. true PTT on, false PTT off
 * @param freq. literal de la frecuencia.
 * @param  dev	Indice del array _SndPorts. El dispositivo (microfono) fuente del audio.
 * @param  PTT_type	Tipo de PTT.
 * @return	0 OK, -1  error.
 */
int RecordPort::RecPTT(bool on, const char *freq, int dev, CORESIP_PttType PTT_type)
{
	bool status_changed;
	PJ_UNUSED_ARG(dev);

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending RecPTT. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}

	if (SetFreqPtt(freq, on, &status_changed))
	{
		PJ_LOG(3,(__FILE__, "ERROR: Adding frequency to record. Resource %s", freq));
		return -1;
	}

	if (!status_changed) return 0;
	
	//Si el estado de PTT de esta frecuencia ha cambiado entonces hay evento válido de PTT
	return RecPTT_send(on, freq, PTT_type);
}

/**
 * RecPTT.	...
 * Envía PTT ON al grabador para que se envíe el mensaje RECORD
 * @param on. true PTT on, false PTT off
 * @param  PTT_type	Tipo de PTT.
 * @return	0 OK, -1  error.
 */
int RecordPort::RecPTT_send(bool on, const char *freq, CORESIP_PttType PTT_type)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		if (on) PJ_LOG(3,(__FILE__, "ERROR: Sending PTT ON. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		else PJ_LOG(3,(__FILE__, "ERROR: Sending PTT OFF. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}

	if (on)
	{
		if(Record(on))
			return -1;
	}

	pj_bzero(mess, sizeof(mess));
	if (on) 
	{		
		strcpy(mess, REC_PTTON);		
	}
	else 
	{	
		strcpy(mess, REC_PTTOFF);
	}
	int len_mess = strlen(mess)+strlen(_RecursoTipoTerminal)+1+strlen(freq)+1+1;
	if (len_mess >= sizeof(mess)) 
	{
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		//nsec_media = 0;		

		strcat(mess, _RecursoTipoTerminal);
		strcat(mess, ",");
		strcat(mess, freq);		
		if (on)
		{
			strcat(mess, ",");
			switch (PTT_type)
			{
			case CORESIP_PTT_COUPLING:
				strcat(mess, "2");
				break;
			case CORESIP_PTT_PRIORITY:
				strcat(mess, "3");
				break;
			case CORESIP_PTT_EMERGENCY:
				strcat(mess, "4");
				break;
			default:
				strcat(mess, "1");
			}
		}

		ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecPTT %s", _RecursoTipoTerminal));	
		}
	}

	if (!on)
	{
		if(Record(on))
			return -1;
	}

	PJ_CHECK_STATUS(st, ("ERROR al enviar PTT %s", _RecursoTipoTerminal));

	return ret;
}

/**
 * RecSQU.	...
 * Envía SQU grabador para que se envíe el mensaje RECORD o PAUSE para recurso radio
 * @param on. true SQU on, false SQU off
 * @param freq. literal de la frecuencia.
 * @param  dev			Indice del array _SndPorts. Es dispositivo (microfono) fuente del audio.
 * @param	resourceId  Identificador del recurso seleccionado en el bss
 * @param	bssMethod	Método bss
 * @param  bssQidx		Indice de calidad
 * @return	0 OK, -1  error.
 */
int RecordPort::RecSQU(bool on, const char *freq, const char *resourceId, const char *bssMethod, unsigned int bssQidx)
{
	bool status_changed;
	bool status_bss_changed;
	int ret = 0;

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending RecSQU. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}

	if (SetFreqSqu(freq, on, &status_changed, resourceId, bssMethod, bssQidx, &status_bss_changed))
	{
		PJ_LOG(3,(__FILE__, "ERROR: Adding frequency to record. Resource %s", freq));
		return -1;
	}

	if (status_changed) ret = RecSQU_send(on, freq);

	/* Lo comento hasta que este implementado este mensaje en el servicio de grabacion
	if (on && status_bss_changed)
	{
		PJ_LOG(5,(__FILE__, "RecSQU: Recurso seleccionado %s Metodo BSS %s QIDX %u", resourceId, bssMethod, bssQidx));	
		RecBSS_send(freq, resourceId, bssMethod, bssQidx);
	}
	*/
		
	return ret;
}

/**
 * RecSQU.	...
 * Envía SQU grabador para que se envíe el mensaje RECORD o PAUSE
 * @param on. true SQU on, false SQU off
 * @return	0 OK, -1  error.
 */
int RecordPort::RecSQU_send(bool on, const char *freq)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		if (on) PJ_LOG(3,(__FILE__, "ERROR: Sending SQUELCH ON. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		else PJ_LOG(3,(__FILE__, "ERROR: Sending SQUELCH OFF. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}

	if (on)
	{
		if (Record(on))
		{
			return -1;
		}
	}
	
	pj_bzero(mess, sizeof(mess));
	if (on) 
	{
		strcpy(mess, SQUELCH_ON);		
	}
	else 
	{	
		strcpy(mess, SQUELCH_OFF);
	}
	int len_mess = strlen(mess)+strlen(_RecursoTipoTerminal)+1+strlen(freq);
	if (len_mess >= sizeof(mess)) 
	{
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		//nsec_media = 0;		

		strcat(mess, _RecursoTipoTerminal);
		strcat(mess, ",");
		strcat(mess, freq);

		ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecSQU %s", _RecursoTipoTerminal));	
		}
	}

	if (!on)
	{
		if (Record(on))
		{
			return -1;
		}
	}

	PJ_CHECK_STATUS(st, ("ERROR al enviar SQUELCH %s", _RecursoTipoTerminal));

	return ret;
}

/**
 * Record.	...
 * Envía RECORD al servicio de grabacion
 * @param on. true = RECORD, false = PAUSE
 * @param force. true Fuerza el envío del comando RECORD
 * @return	0 OK, -1  error.
 */
int RecordPort::Record(bool on)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		if (on) PJ_LOG(3,(__FILE__, "ERROR: Sending RECORD. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		else PJ_LOG(3,(__FILE__, "ERROR: Sending PAUSE. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}	

	pj_bzero(mess, sizeof(mess));
	if (on) 
	{
		strcpy(mess, REC_RECORD);		
	}
	else 
	{	
		strcpy(mess, REC_PAUSE);
	}
	int len_mess = strlen(mess)+strlen(_RecursoTipoTerminal);
	if (len_mess >= sizeof(mess)) 
	{
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		nsec_media = 0;		

		strcat(mess, _RecursoTipoTerminal);
		
		//ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_Command_queue);
		ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_RecPau_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RECORD on=%d, %s", on, _RecursoTipoTerminal));	
		}
	}

	PJ_CHECK_STATUS(st, ("ERROR al enviar RECORD/PAUSE %s", _RecursoTipoTerminal));

	return ret;
}

/**
 * RecBSS_send.	...
 * Envia al grabador el mensaje de recurso seleccionado por BSS
 * @param freq. Frecuencia
 * @param selected_resource. Identificador del recurso seleccionado
 * @param bss_method. Metodo de calculo del qidx
 * @param qidx. Indice de calidad
 * @return	0 OK, -1  error.
 */
int RecordPort::RecBSS_send(const char *freq, const char *selected_resource, const char *bss_method, unsigned int qidx)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	char mess[256];
	char sqidx[8];
	char sbssmethod[2];
	int qidx_dbm;

	if (SessStatus != RECPORT_SESSION_OPEN)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Sending BSS resource. Record Session is not open. Resource %s", _RecursoTipoTerminal));
		return -1;
	}	

	sbssmethod[0] = 0;
	if (strnicmp(bss_method, "RSSI", 4) == 0)
	{
		strcpy(sbssmethod, "0");

		//Se combierte el qidx a dBm si es RSSI
		//0 -> -100dBm, 15 -> -70dBm
		if (qidx > 15) qidx = 15;
		qidx_dbm = ((((-70) - (-100)) * ((int) qidx)) / 15) + (-100);

		snprintf(sqidx, sizeof(sqidx),"%d", qidx_dbm);
		sqidx[sizeof(sqidx)-1] = '\0';
	}
	else
	{
		//ULISES solo soporta RSSI y NUCLEO. 
		strcpy(sbssmethod, "5");
		snprintf(sqidx, sizeof(sqidx),"%u", qidx);
		sqidx[sizeof(sqidx)-1] = '\0';
	}

	pj_bzero(mess, sizeof(mess));
	strcpy(mess, REC_BSS);
	int len_mess = strlen(mess)+strlen(_RecursoTipoTerminal)+1+strlen(freq)+1+
		strlen(selected_resource)+1+strlen(sbssmethod)+1+strlen(sqidx);
	if (len_mess >= sizeof(mess)) 
	{
		st = PJ_ENOMEM;
		ret = -1;
	}

	if (ret == 0)
	{
		strcat(mess, _RecursoTipoTerminal);
		strcat(mess, ",");
		strcat(mess, freq);
		strcat(mess, ",");
		strcat(mess, selected_resource);
		strcat(mess, ",");
		strcat(mess, sbssmethod);
		strcat(mess, ",");
		strcat(mess, sqidx);

		ret = Add_Rec_Command_Queue(mess, len_mess, &Rec_Command_queue);
		if (ret)
		{
			PJ_LOG(3,(__FILE__, "ERROR: Sending RecBSS_send %s", _RecursoTipoTerminal));	
		}
	}

	PJ_CHECK_STATUS(st, ("ERROR al enviar RecBSS %s", _RecursoTipoTerminal));

	return ret;
}


pj_status_t RecordPort::PutFrame(pjmedia_port * port, const pjmedia_frame *frame)
{
	RecordPort * pThis = reinterpret_cast<RecordPort*>(port->port_data.pdata);
	pj_status_t ret = PJ_SUCCESS;

	pj_assert((frame->type != PJMEDIA_FRAME_TYPE_AUDIO) || 
		(frame->size == SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8)));

	Guard lock(pThis->_Lock);

	//Aquí envía al grabador.

	if (frame->size == 0) 
		return ret;

#ifdef DEBUG_GRABACION
	char deb_grab_pay[256];
	char dat_hex_str[8];
	deb_grab_pay[0] = 0;
	for (unsigned int i = 0; i < frame->size/20; i++)
	{
		sprintf(dat_hex_str, "0x%02X,", ((pj_uint8_t *) frame->buf)[i]);
		strcat(deb_grab_pay, dat_hex_str);
	}
	PJ_LOG(3,(__FILE__, "DEBUG: GRAB137 %s %s", pThis->_RecursoTipoTerminal, deb_grab_pay));
#endif

	if (pThis->SessStatus != RECPORT_SESSION_OPEN)
		return ret;

	if (pThis->Resource_type == TEL_RESOURCE && pThis->recording_by_tel == 0)
	{
		pThis->Record(true);
		return ret;			
	}
	else if (pThis->Resource_type == RAD_RESOURCE && pThis->recording_by_rad == 0)
	{
		int recording_by_ptt_aux, recording_by_squ_aux;
		pThis->GetNumSquPtt(&recording_by_ptt_aux, &recording_by_squ_aux);
		if (recording_by_ptt_aux + recording_by_squ_aux > 0)
		{
			pThis->Record(true);
			return ret;
		}
		else return ret;
	}

	if (pThis->t_last_command.msec != 0 && pThis->t_last_command.sec != 0)
	{
		//Se ha enviado un comando. Comprobamos que ha pasado el tiempo
		//necesario antes de volver a enviar media al grabador.
		pj_time_val tv;
		pj_gettimeofday(&tv);
		PJ_TIME_VAL_SUB(tv, pThis->t_last_command);
		pj_uint32_t msec = PJ_TIME_VAL_MSEC(tv);
		if (msec < SLEEP_FOR_SIMU+SLEEP_FOR_SIMU/10)
			return ret;
		pj_bzero(&pThis->t_last_command, sizeof(pThis->t_last_command));
	}

	char snsec[16];
	pj_utoa((unsigned long) pThis->nsec_media, snsec);
		
	pThis->mess_media[0] = 0;
	strcpy(pThis->mess_media, "V,MMM,");

	pj_ssize_t mess_len;
	mess_len = strlen(pThis->mess_media) + strlen(pThis->_RecursoTipoTerminal) + 1 /* , */ + strlen(snsec) + 1 /* , */ + frame->size/2;

#ifdef REC_IN_FILE
	pj_ssize_t mess_len_tx = mess_len - frame->size/2;
#endif

	pj_assert(mess_len <= sizeof(pThis->mess_media));

	strcat(pThis->mess_media, pThis->_RecursoTipoTerminal);
	strcat(pThis->mess_media, ",");
		
	strcat(pThis->mess_media, snsec);
	strcat(pThis->mess_media, ",");
	pThis->nsec_media++;

	char *media_payload = &pThis->mess_media[strlen(pThis->mess_media)];
	pjmedia_alaw_encode((pj_uint8_t *) media_payload, (const pj_int16_t *) frame->buf, frame->size/2);	

	/*
	static pj_uint8_t car = 0;
	int aux = strlen(pThis->mess_media);
	for (int i = 0; i < 160; i++)
	{
		pThis->mess_media[aux+i] = (char) car;
	}	
	car++;
	*/

#ifdef REC_IN_FILE
	pj_ssize_t ss = frame->size / 2;
	ret = pj_file_write(pThis->sim_rec_fd, media_payload, (pj_ssize_t *) &ss);
	//ret = pj_file_write(pThis->sim_rec_fd, frame->buf, (pj_ssize_t *) &frame->size);
	ret = pj_file_write(pThis->sim_rec_tx_fd, pThis->mess_media, (pj_ssize_t *) &mess_len_tx);
	mess_len_tx = 1;
	ret = pj_file_write(pThis->sim_rec_tx_fd, "\n", (pj_ssize_t *) &mess_len_tx);
#endif

	ret = pj_sock_sendto(pThis->_Sock, pThis->mess_media, (pj_ssize_t *) &mess_len, 0, &pThis->recAddr, sizeof(pThis->recAddr));
	if (ret != PJ_SUCCESS)
	{
		char buf[256];
		pj_strerror(ret, buf, sizeof(buf));
		PJ_LOG(3,(__FILE__, "ERROR: pj_sock_sendto PutFrame ERROR SOCK %s, %s", buf, pThis->_RecursoTipoTerminal));
	}
	else
	{
	}

	//pThis->mess_media[30] = 0;
	//PJ_LOG(5,(__FILE__, "TRAZA: %s", pThis->mess_media));

	return ret;	
}

pj_status_t RecordPort::Reset(pjmedia_port * port)
{
	/*RecordPort * pThis = reinterpret_cast<RecordPort*>(port->port_data.pdata);
	Guard lock(pThis->_Lock);

	return pjmedia_delay_buf_reset(pThis->_SndInBufs[port->port_data.ldata]);*/

	return PJ_SUCCESS;
}

/**
 * ConnectRx.	...
 * Conecta/desconecta los puertos de recepcion de audio almacenados en SlotsToSndPorts con el puerto de grabacion
 * Es decir los audios en Rx.
 * @param	on						true - record / false - pause
 * @return	0 OK, -1  error.
 */
int RecordPort::ConnectRx(bool on)
{
	pj_status_t st = PJ_SUCCESS;
	int ret = 0;
	enum {MAXTRIES = 50};

	for (int i = 0; i < MAXTRIES; i++)
	{
		if (pj_mutex_trylock(record_mutex) == PJ_SUCCESS) break;
		pj_thread_sleep(10);
		if (i == (MAXTRIES-1)) return -1;
	}

	if (on)
	{		
		//Conecta los slots de los puertos de telefonia con el puerto de grabacion
		for (int i = 0; i < CORESIP_MAX_SLOTSTOSNDPORTS; i++)
		{
			if (SlotsToSndPorts[i] != PJSUA_INVALID_ID)
			{
				if (!SipAgent::IsSlotValid(SlotsToSndPorts[i]))
				{
					SlotsToSndPorts[i] = PJSUA_INVALID_ID;
				}
				else if (!IsSlotConnectedToRecord(SlotsToSndPorts[i]))
				{
					st = pjsua_conf_connect(SlotsToSndPorts[i], Slot);			
					PJ_CHECK_STATUS(st, ("ERROR conectando puertos", "(%d --> puerto grabacion %d)", SlotsToSndPorts[i], Slot));
				}
			}
		}
	}
	else
	{
		//Desconecta los slots de los puertos de telefonia con el puerto de grabacion
		for (int i = 0; i < CORESIP_MAX_SLOTSTOSNDPORTS; i++)
		{
			if (SlotsToSndPorts[i] != PJSUA_INVALID_ID)
			{
				if (!SipAgent::IsSlotValid(SlotsToSndPorts[i]))
				{
					SlotsToSndPorts[i] = PJSUA_INVALID_ID;
				}
				else if (IsSlotConnectedToRecord(SlotsToSndPorts[i]))
				{
					st = pjsua_conf_disconnect(SlotsToSndPorts[i], Slot);			
					PJ_CHECK_STATUS(st, ("ERROR desconectando puertos", "(%d --> puerto grabacion %d)", SlotsToSndPorts[i], Slot));
				}
			}
		}
	}

	pj_mutex_unlock(record_mutex);
	
	return ret;
}

/**
 * Add_to_TelSlotsToSndPorts. Añade un slot al array TelSlotsToSndPorts
 * Esta funcion se llama cuando se conecta un canal de telefonía con un puerto de sonido en la conf de pjsua. 
 * A su vez los conecta con el puerto de grabacion
 * (En la funcion BridgeLink)
 * @param	slot	slot a añadir.
 * @return	
 */
void RecordPort::Add_SlotsToSndPorts(pjsua_conf_port_id slot)
{
	pj_status_t st = PJ_SUCCESS;
	enum {MAXTRIES = 50};

	if (slot == PJSUA_INVALID_ID)
	{
		return;
	}

	for (int i = 0; i < MAXTRIES; i++)
	{
		if (pj_mutex_trylock(record_mutex) == PJ_SUCCESS) break;
		pj_thread_sleep(10);
		if (i == (MAXTRIES-1)) return;
	}
	
	for (int i = 0; i < CORESIP_MAX_SLOTSTOSNDPORTS; i++)
	{
		if (SlotsToSndPorts[i] == PJSUA_INVALID_ID)
		{	
			if (!IsSlotConnectedToRecord(slot))
			{
				st = pjsua_conf_connect(slot, Slot);			
				PJ_CHECK_STATUS(st, ("ERROR conectando puertos", "(%d --> puerto grabacion %d)", slot, Slot));
			}
			SlotsToSndPorts[i] = slot;
			break;
		}
	}
	pj_mutex_unlock(record_mutex);
}

/**
 * Del_to_TelSlotsToSndPorts. Elimina un slot al array TelSlotsToSndPorts
 * Esta funcion se llama cuando se desconecta un canal de telefonía de un puerto de sonido en la conf de pjsua. 
 * (En la funcion BridgeLink)
 * Si ya está grabando telefonía, 
 * entonces los desconecta del puerto de grabacion VoIP de la telefonia.
 * @param	slot	slot a eliminar.
 * @return	
 */
void RecordPort::Del_SlotsToSndPorts(pjsua_conf_port_id slot)
{
	enum {MAXTRIES = 50};
	int count = 0;	
	int index = -1;

	if (slot == PJSUA_INVALID_ID)
	{
		return;
	}

	for (int i = 0; i < MAXTRIES; i++)
	{
		if (pj_mutex_trylock(record_mutex) == PJ_SUCCESS) break;
		pj_thread_sleep(10);
		if (i == (MAXTRIES-1)) return;
	}

	//Se cuentan las veces que el slot ha sido conectado con el puerto de grabacion
	//Un slot puede haberse conectado varias veces porque puede haberse conectado a varios
	//dispositivos de audio de salida, por ejemplo al microtelefono del instructor y del alumno
	for (int i = 0; i < CORESIP_MAX_SLOTSTOSNDPORTS; i++)
	{
		if (SlotsToSndPorts[i] != PJSUA_INVALID_ID)
		{
			if (SlotsToSndPorts[i] == slot)
			{
				count++;
				index = i;
			}
			else
			{
				if (!IsSlotConnectedToRecord(SlotsToSndPorts[i]))
				{
					//Hacemos limpieza. 
					SlotsToSndPorts[i] = PJSUA_INVALID_ID;
				}
			}
		}
	}

	//Si esta conectado, entonces se quita del array, y si solo está conectado una vez, entonces 
	//se desconecta del puerto de grabacion
	if (index != -1)
	{
		SlotsToSndPorts[index] = PJSUA_INVALID_ID;
		if (count == 1)
		{			
			if (IsSlotConnectedToRecord(slot))
			{
				pj_status_t st = pjsua_conf_disconnect(slot, Slot);												
				PJ_CHECK_STATUS(st, ("ERROR desconectando puertos", "(%d --> puerto grabacion %d)", slot, Slot));
			}
		}
	}

	pj_mutex_unlock(record_mutex);
}

/**
 * IsSlotConnectedToRecord.	...
 * Retorna si el Slot está conectado al puerto de grabación
 * @param	slot. slot a validar
 * @return	true o false.
 */
bool RecordPort::IsSlotConnectedToRecord(pjsua_conf_port_id slot)
{
	if (slot == PJSUA_INVALID_ID)
	{
		return false;
	}

	pjsua_conf_port_info info;
	pj_status_t st = pjsua_conf_get_port_info(slot, &info);
	if (st != PJ_SUCCESS) 
	{
		return false;
	}

	for (unsigned int i = 0; i < info.listener_cnt; i++)
	{
		if (info.listeners[i] == Slot)
		{
			return true;
		}
	}
	return false;
}

/**
 * send_command_timer_cb.	...
 * Callback que se llama cuando send_command_timer expira. Es decir si no se ha recibido ningun mensaje del grabador
 * @return	true o false.
 */
void RecordPort::send_command_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
    pj_status_t st;
	RecordPort *wp = (RecordPort *) te->user_data;

    PJ_UNUSED_ARG(th);

	if (wp->send_command_timer.id == 0)
	{
		pjsua_cancel_timer(&wp->send_command_timer);
		return;
	}

	wp->send_command_timer.id = 0;
	pjsua_cancel_timer(&wp->send_command_timer);

	st = pj_mutex_lock(wp->mutex);
	PJ_CHECK_STATUS(st, ("ERROR pj_mutex_lock en send_command_timer_cb"));
	strcpy(wp->message_received, REC_NOT_MESSAGE);   //El timer a vencido sin que se haya recibido ningún mensaje del grabador
	st = pj_sem_post(wp->sem);
	PJ_CHECK_STATUS(st, ("ERROR pj_sem_post en send_command_timer_cb"));
	st = pj_mutex_unlock(wp->mutex);
	PJ_CHECK_STATUS(st, ("ERROR pj_mutex_unlock en send_command_timer_cb"));
}

/**
 * update_session_timer_cb.	...
 * Callback que se llama cuando update_session_timer expira. Para actualizar el estado de la sesion de grabacion
 * @return	true o false.
 */
void RecordPort::update_session_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
	RecordPort *wp = (RecordPort *) te->user_data;

    PJ_UNUSED_ARG(th);

	if (wp->update_session_timer.id == 0)
	{
		pjsua_cancel_timer(&wp->update_session_timer);
		return;
	}

	wp->update_session_timer.id = 0;
	pjsua_cancel_timer(&wp->update_session_timer);

	wp->RecResetSession();
}

pj_status_t RecordPort::StartSessionTimer(long seconds)
{
	//Se inicia el timer de actualización de la sesion 
	pj_time_val	delay;
	delay.sec = seconds;	
	delay.msec = 0;
	pj_timer_entry_init( &update_session_timer, SES_TIM_ID, (void *) this, update_session_timer_cb);
	return pjsua_schedule_timer(&update_session_timer, &delay);
}

void RecordPort::CancelSessionTimer()
{
	update_session_timer.id = 0;
	pjsua_cancel_timer(&update_session_timer);
}

/**
 * SetFreq.	...
 * Actualiza el estado de ptt y squelch en la tabla frequencies para una frecuencia de radio.
 * @param	freq_lit. Literal de la frecuencia
 * @param	ptt. Nuevo Estado del ptt
 * @param	squ. Nuevo estado del squelch
 * @return	-1 si hay error.
 */
int RecordPort::SetFreq(const char *freq_lit, bool ptt, bool squ, const char *resourceId, const char *bssMethod, unsigned int bssQidx)
{
	int i;	

	if (strlen(freq_lit)+1 > sizeof(frequencies[i].freq_literal)) 
	{
		pj_status_t st = PJ_ENOMEM;
		PJ_CHECK_STATUS(st, ("ERROR al grabar frecuencia %s", freq_lit));
		return -1;
	}

	//Busca la frecuencia. Si existe entonces le asigna estados de ptt y squ
	for (i = 0; i < MAX_NUM_FREQUENCIES; i++)
	{
		if (strcmp(freq_lit, frequencies[i].freq_literal) == 0)
		{
			frequencies[i].ptt = ptt;
			frequencies[i].squ = squ;

			if (squ)
			{
				//Si squelch es on, le asignamos los parametros de bss
				strncpy(frequencies[i].resourceId, resourceId, sizeof(frequencies[i].resourceId));
				frequencies[i].resourceId[sizeof(frequencies[i].resourceId) - 1] = '\0';
				strncpy(frequencies[i].bssMethod, bssMethod, sizeof(frequencies[i].bssMethod));
				frequencies[i].bssMethod[sizeof(frequencies[i].bssMethod) - 1] = '\0';
				frequencies[i].bssQidx = bssQidx;
			}
			else
			{
				//Si squelch es off, le borramos los parametros de bss
				frequencies[i].resourceId[0] = '\0';
				frequencies[i].bssMethod[0] = '\0';
				frequencies[i].bssQidx = 0;
			}

			if (!frequencies[i].ptt && !frequencies[i].squ)
			{
				//Si se desactivan tanto ptt como squ entonces borramos la 
				//frecuencia del array. Solo se guarda es estado de una frecuancia si ptt o squ está a on.
				//Si los dos están a off se elimina el elemento de esa frecuencia en el array
				memset(&frequencies[i], 0, sizeof(frequencies[i]));
			}

			return 0;
		}
	}

	if (!ptt && !squ)
	{
		return 0;		//Si no existe la frecuencia en el array y se quiere
						//desactivar tanto ptt como squ entonces no se añade ningun elemento
	}

	//Busca una posicion libre y añade el literal y sus estados
	for (i = 0; i < MAX_NUM_FREQUENCIES; i++)
	{
		if (strlen(frequencies[i].freq_literal) == 0)
		{
			strcpy(frequencies[i].freq_literal, freq_lit);
			frequencies[i].ptt = ptt;
			frequencies[i].squ = squ; 
			if (squ)
			{
				strncpy(frequencies[i].resourceId, resourceId, sizeof(frequencies[i].resourceId));
				frequencies[i].resourceId[sizeof(frequencies[i].resourceId) - 1] = '\0';
				strncpy(frequencies[i].bssMethod, bssMethod, sizeof(frequencies[i].bssMethod));
				frequencies[i].bssMethod[sizeof(frequencies[i].bssMethod) - 1] = '\0';
				frequencies[i].bssQidx = bssQidx;
			}
			else
			{
				//Si squelch es off, le borramos los parametros de bss
				frequencies[i].resourceId[0] = '\0';
				frequencies[i].bssMethod[0] = '\0';
				frequencies[i].bssQidx = 0;
			}
			return 0;
		}
	}

	return -1;
}

int RecordPort::GetFreq(const char *freq_lit, bool *ptt, bool *squ, char *resourceId, char *bssMethod, unsigned int *bssQidx)
{
	*ptt = false;
	*squ = false;	

	resourceId[0] = '\0';
	bssMethod[0] = '\0';
	*bssQidx = 0;
	
	for (int i = 0; i < MAX_NUM_FREQUENCIES; i++)
	{
		if (strcmp(freq_lit, frequencies[i].freq_literal) == 0)
		{
			*ptt = frequencies[i].ptt;
			*squ = frequencies[i].squ;
			strncpy(resourceId, frequencies[i].resourceId, MAX_RESOURCEID_LITERAL);
			resourceId[MAX_RESOURCEID_LITERAL - 1] = '\0';
			strncpy(bssMethod, frequencies[i].bssMethod, MAX_BSSMETHOD_LITERAL);
			bssMethod[MAX_BSSMETHOD_LITERAL - 1] = '\0';
			*bssQidx = frequencies[i].bssQidx;
			break;
		}
	}

	return 0;
}

int RecordPort::SetFreqPtt(const char *freq_lit, bool ptt, bool *status_changed)
{
	int ret = 0;
	bool ptta1, ptta2, squa;
	char resourceId_aux[MAX_RESOURCEID_LITERAL];
	char bssMethod_aux[MAX_BSSMETHOD_LITERAL];
	unsigned int bssQidx_aux;

	*status_changed = false;

	pj_mutex_lock(frequencies_mutex);
	GetFreq(freq_lit, &ptta1, &squa, resourceId_aux, bssMethod_aux, &bssQidx_aux);
	ret = SetFreq(freq_lit, ptt, squa, resourceId_aux, bssMethod_aux, bssQidx_aux);
	if (ret) 
	{
		pj_mutex_unlock(frequencies_mutex);
		return ret;
	}
	GetFreq(freq_lit, &ptta2, &squa, resourceId_aux, bssMethod_aux, &bssQidx_aux);
	pj_mutex_unlock(frequencies_mutex);
	
	if (ptta1 == ptta2) 
		*status_changed = false;
	else
		*status_changed = true;

	return ret;
}

/**
 * SetFreqSqu.	...
 * Actualiza el estado de squelch en la tabla frequencies para una frecuencia de radio.
 * @param	freq_lit. Literal de la frecuencia
 * @param	squ. Nuevo estado del squelch
 * @param	status_changed. Retorna true si el estado de squelch ha cambiado para esa frecuencia
 * @param	resourceId  Identificador del recurso seleccionado en el bss
 * @param	bssMethod. Actualiza el bss method
 * @param	bssQidx. Actualiza el qidx
 * @param	status_bss_changed. Retorna true si han cambiado los parametros de bss en esa frecuencia.
 * @return	-1 si hay error.
 */
int RecordPort::SetFreqSqu(const char *freq_lit, bool squ, bool *status_changed, 
						   const char *resourceId, const char *bssMethod, unsigned int bssQidx, bool *status_bss_changed)
{
	int ret = 0;
	bool ptta, squa1, squa2;

	char bssMethod_aux[MAX_BSSMETHOD_LITERAL], bssMethod_aux2[MAX_BSSMETHOD_LITERAL];
	char resourceId_aux[MAX_RESOURCEID_LITERAL], resourceId_aux2[MAX_RESOURCEID_LITERAL];
	unsigned int bssQidx_aux, bssQidx_aux2;

	*status_changed = false;
	*status_bss_changed = false;

	pj_mutex_lock(frequencies_mutex);
	GetFreq(freq_lit, &ptta, &squa1, resourceId_aux, bssMethod_aux, &bssQidx_aux);
	ret = SetFreq(freq_lit, ptta, squ, resourceId, bssMethod, bssQidx);
	if (ret) 
	{
		pj_mutex_unlock(frequencies_mutex);
		return ret;
	}
	GetFreq(freq_lit, &ptta, &squa2, resourceId_aux2, bssMethod_aux2, &bssQidx_aux2);
	pj_mutex_unlock(frequencies_mutex);

	if (squa1 == squa2)
		*status_changed = false;
	else
		*status_changed = true;

	//Se enviará el evento al grabador siempre que haya cambiado el recurso seleccionado o el metodo.
	if ((strncmp(resourceId_aux, resourceId_aux2, MAX_RESOURCEID_LITERAL) == 0) &&
		(strncmp(bssMethod_aux, bssMethod_aux2, MAX_BSSMETHOD_LITERAL) == 0))
		*status_bss_changed = false;
	else
		*status_bss_changed = true;

	return ret;
}

void RecordPort::GetNumSquPtt(int *nPtt, int *nSqu)
{
	*nPtt = 0;
	*nSqu = 0;

	pj_mutex_lock(frequencies_mutex);
	for (int i = 0; i < MAX_NUM_FREQUENCIES; i++)
	{
		*nPtt += frequencies[i].ptt ? 1 : 0;
		*nSqu += frequencies[i].squ ? 1 : 0;
	}
	pj_mutex_unlock(frequencies_mutex);
}

/**
 * GetTelNum.	...
 * Extrae el numero de telefono de la URI
 * @param	uri. 
 * @param   uri_len. Longitud del string uri
 * @param   tel. Puntero donde retorna el telefono
 * @param   tel_len. Longitud del buffer tel
 * @return	true o false.
 */
void RecordPort::GetTelNum(char *uri, int uri_len, char *tel, int tel_len)
{
	memset(tel, 0, tel_len);

	pjsip_uri* pjuri;

	pj_pool_t *tmppool = pjsua_pool_create("GetTelNum", 64, 32);
	if (tmppool == NULL)
	{
		PJ_LOG(3,(__FILE__, "ERROR: Memoria insuficiente en RecordPort::GetTelNum")); 	
		return;
	}

	/*Se crea un string duplicado para el parse, ya que se ha visto que
	pjsip_parse_uri puede modificar el parametro de entrada*/
	pj_str_t uri_aux;
	uri_aux.ptr = uri;
	uri_aux.slen = uri_len;
	pj_str_t uri_dup;
	pj_strdup_with_null(tmppool, &uri_dup, &uri_aux);
	
	pjuri = pjsip_parse_uri(tmppool, uri_dup.ptr, uri_dup.slen, 0);
	if (pjuri == NULL)
	{
		PJ_LOG(3,(__FILE__, "ERROR: RecordPort::GetTelNum Uri no valida %s", uri));
		pj_pool_release(tmppool);
		return;
	}

	/*Se extrae user*/
	pjsip_sip_uri *url=(pjsip_sip_uri*)pjsip_uri_get_uri(pjuri);
	if (url != NULL)
	{
		if ((url->user.slen+1) > tel_len)
		{
			PJ_LOG(3,(__FILE__, "ERROR: RecordPort::GetTelNum. La longitud del buffer tel de salida es insuficiente.")); 	
			pj_pool_release(tmppool);
			return;
		}
	
		if (url->user.ptr != NULL && url->user.slen > 0)
		{
			memcpy(tel, url->user.ptr, url->user.slen);
		}
	}

	pj_pool_release(tmppool);
}





