/**
 * @file Exports.cpp
 * @brief Interfaz de usuario a CORESIP.dll
 *
 *	Implementacion de las rutinas de interfaz con la DLL
 *
 *	@addtogroup CORESIP
 */
/*@{*/
#include "Global.h"
#include "Exceptions.h"
#include "SipAgent.h"
#include "SipCall.h"
#include "ExtraParamAccId.h"
#include "wg67subscription.h"
#include "WavPlayerToRemote.h"

#define Try\
	pj_thread_desc desc;\
	pj_thread_t * th = NULL;\
	if (!pj_thread_is_registered())\
	{\
		pj_thread_register(NULL, desc, &th);\
	}\
	try

#define catch_all\
	catch (PJLibException & ex)\
	{\
		ret = ex.SetError(error);\
	}\
	catch (...)\
	{\
		if (error)\
		{\
			error->Code = 1;\
			pj_ansi_strcpy(error->File, __FILE__);\
			pj_ansi_strcpy(error->Info, "Unknown exception");\
		}\
		ret = CORESIP_ERROR;\
	}\
	if (th != NULL)\
	{\
		pj_thread_unregister();\
	}

#ifdef _WIN32
#include <windows.h>

/**
 *	Punto de Entrada Principal de la Libreria.
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hinstDLL);
		break;
	}

	return TRUE;
}

#endif

/**
 *	Rutina de Inicializacion del Modulo. @ref SipAgent::Init
 *	@param	cfg		Puntero @ref CORESIP_Config a la configuracion. 
 *	@param	error	Puntero @ref CORESIP_Error a la estructura de Error
 *	@return			Codigo de Error
 */
CORESIP_API int CORESIP_Init(const CORESIP_Config * cfg, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::Init(cfg);

		//UNIFETM: En el etm si retorna error 130049 (el bind con la ip da error) entonces lo reintenta con la localhost

	}
	catch_all;

	return ret;
}

/**
 *	Rutina de Arranque del Modulo. @ref SipAgent::Start
 *	@param	error	Puntero a la estructura de Error. @ref CORESIP_Error
 *	@return			Codigo de Error
 */
CORESIP_API int CORESIP_Start(CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::Start();
	}
	catch_all;

	return ret;
}

/**
 *	Rutina de Parada del Modulo. @ref SipAgent::Stop
 *	@return			Sin Retorno
 */
CORESIP_API void CORESIP_End()
{
	int ret = CORESIP_OK;
	CORESIP_Error * error = NULL;

	Try
	{
		SipAgent::Stop();
	}
	catch_all;
}

/**
 * CORESIP_SetSipPort. Establece el puerto SIP
 * @param	port	Puerto SIP
 * @return			Codigo de Error
 */
CORESIP_API int	CORESIP_SetSipPort(int port, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::SetSipPort(port);
	}
	catch_all;

	return ret;
}

/**
 *	Establece el nivel de LOG del Modulo. @ref SipAgent::SetLogLevel
 *	@param	level	Nivel de LOG solicitado
 *	@param	error	Puntero @ref CORESIP_Error a la estructura de Error. 
 *	@return			Codigo de Error
 */
CORESIP_API int CORESIP_SetLogLevel(unsigned level, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::SetLogLevel(level);
	}
	catch_all;

	return ret;
}

/**
 *	Establece los Parametros del Modulo. @ref SipAgent::SetParams
 *	@param	info	Puntero @ref CORESIP_Params a la estructura de parametros. 
 *	@param	error	Puntero @ref CORESIP_Error a la estructura de Error. 
 *	@return			Codigo de Error
 */
CORESIP_API int CORESIP_SetParams(const CORESIP_Params * info, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::SetParams(info);
	}
	catch_all;

	return ret;
}

/**
 *	CreateAccount. Registra una cuenta SIP en el Módulo. @ref SipAgent::CreateAccount
 *	@param	acc			Puntero a la sip URI que se crea como agente. 
 *	@param	defaultAcc	Marca si esta cuenta pasa a ser la Cuenta por Defecto.
 *	@param	accId		Puntero a el identificador de cuenta asociado.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateAccount(const char * acc, int defaultAcc, int * accId, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*accId = (SipAgent::CreateAccount(acc, defaultAcc) | CORESIP_ACC_ID);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_CreateAccountProxyRouting. Registra una cuenta SIP en el Módulo y los paquetes sip se enrutan por el proxy. @ref SipAgent::CreateAccount
 *	@param	acc			Puntero a la sip URI que se crea como agente.
 *	@param	defaultAcc	Marca si esta cuenta pasa a ser la Cuenta por Defecto.
 *	@param	accId		Puntero a el identificador de cuenta asociado.
 *  @param	proxy_ip	Si es distinto de NULL. IP del proxy Donde se quieren enrutar los paquetes.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateAccountProxyRouting(const char * acc, int defaultAcc, int * accId, const char *proxy_ip, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*accId = (SipAgent::CreateAccount(acc, defaultAcc, proxy_ip) | CORESIP_ACC_ID);
	}
	catch_all;

	return ret;
}

/**
 *	CreateAccountAndRegisterInProxy. Crea una cuenta y se registra en el SIP proxy. Los paquetes sip se rutean por el SIP proxy también.
 *	@param	acc			Puntero al Numero de Abonado (usuario). NO a la uri.
 *	@param	defaultAcc	Si es diferente a '0', indica que se creará la cuenta por Defecto.
 *	@param	accId		Puntero a el identificador de cuenta asociado que retorna.
 *	@param	proxy_ip	IP del proxy.
 *	@param	expire_seg  Tiempo en el que expira el registro en segundos.
 *	@param	username	Si no es necesario autenticación, este parametro será NULL
 *	@param  pass		Password. Si no es necesario autenticación, este parametro será NULL
 *	@param  DisplayName	Display name que va antes de la sip URI, se utiliza para como nombre a mostrar
 *	@param	isfocus		Si el valor es distinto de cero, indica que es Focus, para establecer llamadas multidestino
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateAccountAndRegisterInProxy(const char * acc, int defaultAcc, int * accId, const char *proxy_ip, 
														unsigned int expire_seg, const char *username, const char *pass, const char * displayName, int isfocus, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*accId = (SipAgent::CreateAccountAndRegisterInProxy(acc, defaultAcc, proxy_ip, expire_seg, username, pass, displayName, isfocus) | CORESIP_ACC_ID);
	}
	catch_all;

	return ret;
}

/**
 *	DestroyAccont. Elimina una cuenta SIP del modulo. @ref SipAgent::DestroyAccount
 *	@param	accId		Identificador de la cuenta.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_DestroyAccount(int accId, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		pj_assert((accId & CORESIP_ID_TYPE_MASK) == CORESIP_ACC_ID);
		SipAgent::DestroyAccount(accId & CORESIP_ID_MASK);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_SetTipoGRS. Configura el tipo de GRS. El ETM lo llama cuando crea un account tipo GRS.
 *	@param	accId		Identificador de la cuenta.
 *	@param	FlagGRS	Tipo de GRS.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_SetTipoGRS(int accId, CORESIP_CallFlags FlagGRS, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		pj_assert((accId & CORESIP_ID_TYPE_MASK) == CORESIP_ACC_ID);
		SipAgent::SetTipoGRS(accId & CORESIP_ID_MASK, FlagGRS);
	}
	catch_all;

	return ret;
}

/**
 *	AddSndDevice		Añade un dispositvo de audio al módulo. @ref SipAgent::AddSndDevice
 *	@param	info		Puntero @ref CORESIP_SndDeviceInfo a la Informacion asociada al dispositivo.
 *	@param	dev			Puntero donde se recorre el identificador del dispositivo.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_AddSndDevice(const CORESIP_SndDeviceInfo * info, int * dev, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*dev = (SipAgent::AddSndDevice(info) | CORESIP_SNDDEV_ID);
	}
	catch_all;

	return ret;
}

/**
 *	CreateWavPlayer		Crea un 'Reproductor' WAV. @ref SipAgent::CreateWavPlayer
 *	@param	file		Puntero al path del fichero.
 *	@param	loop		Marca si se reproduce una sola vez o indefinidamente.
 *	@param	wavPlayer	Puntero donde se recorre el identificador del 'reproductor'.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateWavPlayer(const char * file, unsigned loop, int * wavPlayer, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*wavPlayer = (SipAgent::CreateWavPlayer(file, loop != 0) | CORESIP_WAVPLAYER_ID);
	}
	catch_all;

	return ret;
}

/**
 *	DestroyWavPlayer	Elimina un Reproductor WAV. @ref SipAgent::DestroyWavPlayer
 *	@param	wavPlayer	Identificador del Reproductor.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_DestroyWavPlayer(int wavPlayer, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		//pj_assert((wavPlayer & CORESIP_ID_TYPE_MASK) == CORESIP_WAVPLAYER_ID);
		if((wavPlayer & CORESIP_ID_TYPE_MASK) == CORESIP_WAVPLAYER_ID)
			SipAgent::DestroyWavPlayer(wavPlayer & CORESIP_ID_MASK);
	}
	catch_all;

	return ret;
}

/**
 *	CreateWavRecorder	Crea un 'grabador' en formato WAV. @ref SipAgent::CreateWavRecorder
 *	@param	file		Puntero al path del fichero, donde guardar el sonido.
 *	@param	wavRecorder	Puntero donde se recoge el identificador del 'grabador'
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateWavRecorder(const char * file, int * wavRecorder, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*wavRecorder = (SipAgent::CreateWavRecorder(file) | CORESIP_WAVRECORDER_ID);
	}
	catch_all;

	return ret;
}

/**
 *	DestroyWavRecorder	Elimina un 'grabador' WAV. @ref SipAgent::DestroyWavRecorder
 *	@param	wavRecorder	Identificador del Grabador.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_DestroyWavRecorder(int wavRecorder, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		//pj_assert((wavRecorder & CORESIP_ID_TYPE_MASK) == CORESIP_WAVRECORDER_ID);
		if ((wavRecorder & CORESIP_ID_TYPE_MASK) == CORESIP_WAVRECORDER_ID)
			SipAgent::DestroyWavRecorder(wavRecorder & CORESIP_ID_MASK);
	}
	catch_all;

	return ret;
}


/**
 *	CreateRedRxPort		Crea un 'PORT' @ref RdRxPort de Recepcion Radio. @ref SipAgent::CreateRdRxPort
 *	@param	info		Puntero @ref CORESIP_RdRxPortInfo a la informacion del puerto
 *	@param	localIp		Puntero a la Ip Local.
 *	@param	rdRxPort	Puntero que recoge el identificador del puerto.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateRdRxPort(const CORESIP_RdRxPortInfo * info, const char * localIp, int * rdRxPort, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*rdRxPort = (SipAgent::CreateRdRxPort(info, localIp) | CORESIP_RDRXPORT_ID);
	}
	catch_all;

	return ret;
}

/**
 *	DestroyRdRxPort		Elimina un Puerto @ref RdRxPort. @ref SipAgent::DestroyRdRxPort
 *	@param	rdRxPort	Identificador del Puerto.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_DestroyRdRxPort(int rdRxPort, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		pj_assert((rdRxPort & CORESIP_ID_TYPE_MASK) == CORESIP_RDRXPORT_ID);
		SipAgent::DestroyRdRxPort(rdRxPort & CORESIP_ID_MASK);
	}
	catch_all;

	return ret;
}

/**
 *	CreateSndRxPort.	Crea un puerto @ref SoundRxPort. @ref SipAgent::CreateSndRxPort
 *	@param	id			Puntero al nombre del puerto.
 *	@param	sndRxPort	Puntero que recoge el identificador del puerto.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateSndRxPort(const char * id, int * sndRxPort, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*sndRxPort = (SipAgent::CreateSndRxPort(id) | CORESIP_SNDRXPORT_ID);
	}
	catch_all;

	return ret;
}

/**
 *	DestroySndRxPort	Eliminar un puerto @ref SoundRxPort. @ref SipAgent::DestroySndRxPort
 *	@param	sndRxPort	Identificador del puerto.
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_DestroySndRxPort(int sndRxPort, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		pj_assert((sndRxPort & CORESIP_ID_TYPE_MASK) == CORESIP_SNDRXPORT_ID);
		SipAgent::DestroySndRxPort(sndRxPort & CORESIP_ID_MASK);
	}
	catch_all;

	return ret;
}

/**
 *	BridgeLink			Configura un enlace de conferencia. @ref SipAgent::BridgeLink
 *	@param	src			Tipo e Identificador de Puerto Origen. @ref CORESIP_ID_TYPE_MASK, @ref CORESIP_ID_MASK
 *	@param	dst			Tipo e Identificador de Puerto Destino. @ref CORESIP_ID_TYPE_MASK, @ref CORESIP_ID_MASK
 *	@param	on			Indica Conexión o Desconexión.
 *	@param	error		Puntero @ref CORESIP_Error a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_BridgeLink(int src, int dst, int on, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::BridgeLink(src & CORESIP_ID_TYPE_MASK, src & CORESIP_ID_MASK, 
			dst & CORESIP_ID_TYPE_MASK, dst & CORESIP_ID_MASK, on != 0);
	}
	catch_all;

	return ret;
}

/**
 *	SendToRemote		Configura El puerto de Sonido apuntado para los envios UNICAST de Audio. @ref SipAgent::SendToRemote
 *	@param	dev			...
 *	@param	on			...
 *	@param	id			Puntero a ...
 *	@param	ip			Puntero a ...
 *	@param	port		...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_SendToRemote(int dev, int on, const char * id, const char * ip, unsigned port, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::SendToRemote(dev & CORESIP_ID_TYPE_MASK, dev & CORESIP_ID_MASK, on != 0, id, ip, port);
	}
	catch_all;

	return ret;
}

/**
 *	ReceiveFromRemote
 *	@param	localIp		Puntero a ...
 *	@param	mcastIp		Puntero a ...
 *	@param	mcastPort	...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_ReceiveFromRemote(const char * localIp, const char * mcastIp, unsigned mcastPort, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::ReceiveFromRemote(localIp, mcastIp, mcastPort);
	}
	catch_all;

	return ret;
}

/**
 *	SetVolume
 *	@param	id			...
 *	@param	volume		...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_SetVolume(int id, unsigned volume, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::SetVolume(id & CORESIP_ID_TYPE_MASK, id & CORESIP_ID_MASK, volume);
	}
	catch_all;

	return ret;
}

/**
 *	...
 *	@param	id			...
 *	@param	volume		Puntero a ...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_GetVolume(int id, unsigned * volume, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*volume = SipAgent::GetVolume(id & CORESIP_ID_TYPE_MASK, id & CORESIP_ID_MASK);
	}
	catch_all;

	return ret;
}

/**
 *	CallMake
 *	@param	info		Puntero a la informacion de llamada
 *	@param	outInfo		
 *	@param	call		Puntero a ...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CallMake(const CORESIP_CallInfo * info, const CORESIP_CallOutInfo * outInfo, int * call, CORESIP_Error * error)
{
	int ret = CORESIP_OK;	

	Try
	{
		*call = (SipCall::New(info, outInfo) | CORESIP_CALL_ID);

		//if (info->Type == CORESIP_CALL_IA && outInfo->DstUri[10] == '2') SipAgent::CreatePresenceSubscription("sip:314001@192.168.1.112");
		//else if (info->Type == CORESIP_CALL_DIA && info->Priority == CORESIP_PR_EMERGENCY) SipAgent::DestroyPresenceSubscription("sip:314001@192.168.1.112");
		//if (info->Type == CORESIP_CALL_IA) SipAgent::CreateConferenceSubscription(0, "sip:314005@192.168.1.112");
		//if (info->Type == CORESIP_CALL_IA) SipAgent::SendInstantMessage(0, "sip:314005@192.168.1.112", "Prueba de enviar mensaje de texto", PJ_FALSE);
		//if (info->Type == CORESIP_CALL_IA && outInfo->DstUri[10] == '1') SipAgent::CreateDialogSubscription(0, "\"S5\" <sip:314005@192.168.1.112>", PJ_TRUE);
		//else if (info->Type == CORESIP_CALL_IA && outInfo->DstUri[10] == '2') SipAgent::DestroyDialogSubscription("sip:314005@192.168.1.112");
		//else *call = (SipCall::New(info, outInfo) | CORESIP_CALL_ID);
		
	}
	catch_all;

	return ret;
}

/**
 *	CallAnswer
 *	@param	call		Identificador de Llamada
 *	@param	code		...
 *	@param	addToConference		...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CallAnswer(int call, unsigned code, int addToConference, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		//pj_assert((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID);
		if ((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID)
			SipCall::Answer(call & CORESIP_ID_MASK, code, addToConference != 0);
		else
		{
			ret = CORESIP_ERROR;
			error->Code = 0;
			strcpy(error->File, __FILE__);
			sprintf(error->Info, "CORESIP_CallAnswer: Invalid call id 0x%X", call);
		}
	}
	catch_all;

	return ret;
}

/**
 *	CallHangup
 *	@param	call		Identificador de Llamada
 *	@param	code		...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CallHangup(int call, unsigned code, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		//pj_assert((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID);
		if ((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID)
			SipCall::Hangup(call & CORESIP_ID_MASK, code);
		else
		{
			ret = CORESIP_ERROR;
			error->Code = 0;
			strcpy(error->File, __FILE__);
			sprintf(error->Info, "CORESIP_CallHangup: Invalid call id 0x%X", call);
		}
	}
	catch_all;

	return ret;
}

/**
 *	CallHold
 *	@param	call		Identificador de llamada
 *	@param	hold		...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CallHold(int call, int hold, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		//pj_assert((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID);
		if ((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID)
			SipCall::Hold(call & CORESIP_ID_MASK, hold != 0);
		else
		{
			ret = CORESIP_ERROR;
			error->Code = 0;
			strcpy(error->File, __FILE__);
			sprintf(error->Info, "CORESIP_CallHold: Invalid call id 0x%X", call);
		}
	}
	catch_all;

	return ret;
}

/**
 *	CallTransfer
 *	@param	call		Identificador de llamada
 *	@param	dstCall		...
 *	@param	dst			Puntero a ...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CallTransfer(int call, int dstCall, const char * dst, const char *display_name, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		//pj_assert((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID);
		if ((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID)
			SipCall::Transfer(call & CORESIP_ID_MASK, dstCall != PJSUA_INVALID_ID ? dstCall & CORESIP_ID_MASK : PJSUA_INVALID_ID, dst, display_name);
		else
		{
			ret = CORESIP_ERROR;
			error->Code = 0;
			strcpy(error->File, __FILE__);
			sprintf(error->Info, "CORESIP_CallTransfer: Invalid call id 0x%X", call);
		}
	}
	catch_all;

	return ret;
}

/**
 *	CallPtt
 *	@param	call		Identificador de llamada
 *	@param	info		Puntero a la Informacion asociada al PTT
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CallPtt(int call, const CORESIP_PttInfo * info, CORESIP_Error * error)
{
	//UNIFETM: Esta funcion tiene que utilizarse en el ETM tambien para Squelch

	int ret = CORESIP_OK;

	Try
	{
		//pj_assert((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID);
		if ((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID)
			SipCall::Ptt(call & CORESIP_ID_MASK, info);
		else
		{
			ret = CORESIP_ERROR;
			error->Code = 0;
			strcpy(error->File, __FILE__);
			sprintf(error->Info, "CORESIP_CallPtt: Invalid call id 0x%X", call);
		}
	}
	catch_all;

	return ret;
}

/**
 *	CallSendInfo
 *	@param	call		Identificador de llamada
 *	@param	info		Puntero a ...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CallSendInfo(int call, const char * info, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{		
		//pj_assert((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID);
		if ((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID)
			SipCall::SendInfoMsg(call & CORESIP_ID_MASK, info);
		else
		{
			ret = CORESIP_ERROR;
			error->Code = 0;
			strcpy(error->File, __FILE__);
			sprintf(error->Info, "CORESIP_CallSendInfo: Invalid call id 0x%X", call);
		}
	}
	catch_all;

	return ret;
}

/**
 *	CallConference
 *	@param	call		Identificador de llamada
 *	@param	conf		Identificador de conferencia
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CallConference(int call, int conf, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		//pj_assert((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID);
		if ((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID)
			SipCall::Conference(call & CORESIP_ID_MASK, conf != 0);
		else
		{
			ret = CORESIP_ERROR;
			error->Code = 0;
			strcpy(error->File, __FILE__);
			sprintf(error->Info, "CORESIP_CallConference: Invalid call id 0x%X", call);
		}
	}
	catch_all;

	return ret;
}

/**
 *	CallSendConfInfo
 *	@param	call		Identificador de llamada
 *	@param	info		Puntero a ...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CallSendConfInfo(int call, const CORESIP_ConfInfo * info, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		//pj_assert((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID);
		if ((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID)
			SipCall::SendConfInfo(call & CORESIP_ID_MASK, info);
		else
		{
			ret = CORESIP_ERROR;
			error->Code = 0;
			strcpy(error->File, __FILE__);
			sprintf(error->Info, "CORESIP_CallSendConfInfo: Invalid call id 0x%X", call);
		}
	}
	catch_all;

	return ret;
}

/**
 *	CallSendConfInfo
 *	@param	accId		Identificador del account del agente
 *	@param	info		Puntero a ...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_SendConfInfoFromAcc(int accId, const CORESIP_ConfInfo * info, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		pj_assert((accId & CORESIP_ID_TYPE_MASK) == CORESIP_ACC_ID);
		ExtraParamAccId::SendConfInfoFromAcc(accId & CORESIP_ID_MASK, info);
	}
	catch_all;

	return ret;
}

/**
 *	TransferAnswer
 *	@param	tsxKey		Puntero a ...
 *	@param	txData		Puntero a ...
 *	@param	evSub		Puntero a ...
 *	@param	code		...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_TransferAnswer(const char * tsxKey, void * txData, void * evSub, unsigned code, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipCall::TransferAnswer(tsxKey, txData, evSub, code);
	}
	catch_all;

	return ret;
}

/**
 *	TransferNotify
 *	@param	evSub		Puntero a ...
 *	@param	code		...
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_TransferNotify(void * evSub, unsigned code, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipCall::TransferNotify(evSub, code);
	}
	catch_all;

	return ret;
}

/**
 *	SendOptionsMsg
 *  Esta función no envia OPTIONS a traves del proxy
 *	@param	dst			Puntero a uri donde enviar OPTIONS
 *  @param	callid		callid que retorna.
 *  @param	isRadio		Si tiene valor distinto de cero el agente se identifica como radio. Si es cero, como telefonia.
 *						Sirve principalmente para poner radio.01 o phone.01 en la cabecera WG67-version
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_SendOptionsMsg(const char * dst, char * callid, int isRadio, CORESIP_Error * error)
{
	//UNIFETM: Esta funcion tiene parametros diferentes. Cambiar en ETM para que sea como en ULISES

	int ret = CORESIP_OK;

	Try
	{
		SipCall::SendOptionsMsg(dst, callid, isRadio);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_SendOptionsMsgProxy
 *  Envia OPTIONS a traves del proxy.
 *	@param	dst			Puntero a uri donde enviar OPTIONS
 *  @param	callid		callid que retorna.
 *  @param	isRadio		Si tiene valor distinto de cero el agente se identifica como radio. Si es cero, como telefonia.
 *						Sirve principalmente para poner radio.01 o phone.01 en la cabecera WG67-version
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_SendOptionsMsgProxy(const char * dst, char * callid, int isRadio, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipCall::SendOptionsMsg(dst, callid, isRadio, PJ_TRUE);
	}
	catch_all;

	return ret;
}

/**
 *	CreateWG67Subscription
 *	@param	dst						Puntero a ...
 *	@param	wg67Subscription		Puntero a la Estructura r
 *	@param	error					Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateWG67Subscription(const char * dst, void ** wg67Subscription, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		*wg67Subscription = new WG67Subscription(dst);
	}
	catch_all;

	return ret;
}

/**
 *	DestroyWG67Subscription
 *	@param	wg67Subscription		Puntero a la Estructura r
 *	@param	error					Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_DestroyWG67Subscription(void * wg67Subscription, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		WG67Subscription * object = reinterpret_cast<WG67Subscription*>(wg67Subscription);
		delete object;
	}
	catch_all;

	return ret;
}


/** AGL. Para el SELCAL */
/**
 *	
 *	@param	
 *	@param	
 *	@return
 */
CORESIP_API int CORESIP_Wav2RemoteStart(const char *filename, const char * id, const char * ip, unsigned port, void (*eofCb)(void *), CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		/**
		WavPlayerToRemote *wp=new WavPlayerToRemote(filename, PTIME, eofCb);
		wp->Send2Remote(id, ip, port);
		*/
		SipAgent::CreateWavPlayer2Remote(filename, id, ip, port);
	}
	catch_all;
	return ret;
}

/**
 *	
 *	@param	
 *	@param	
 *	@return
 */
CORESIP_API int CORESIP_Wav2RemoteEnd(void *obj, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		/**
		WavPlayerToRemote *wp = (WavPlayerToRemote *)obj;
		delete wp;
		*/
		SipAgent::DestroyWavPlayer2Remote();
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_RdPttEvent. Se llama cuando hay un evento de PTT en el HMI. Sirve sobretodo para enviar los metadata de grabacion VoIP en el puesto
 *  @param  on			true=ON/false=OFF
 *	@param	freqId		Identificador de la frecuencia 
 *  @param  dev			Indice del array _SndPorts. Es dispositivo (microfono) fuente del audio.
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_RdPttEvent(bool on, const char *freqId, int dev, CORESIP_Error * error, CORESIP_PttType PTT_type) 
{
	int ret = CORESIP_OK;

	Try
	{
		dev &= ~CORESIP_SNDDEV_ID;
		SipAgent::RdPttEvent(on, freqId, dev, PTT_type);		
	}
	catch_all;

	return ret;
}


/**
 *	CORESIP_RdSquEvent. Se llama cuando hay un evento de Squelch en el HMI. Sirve sobretodo para enviar los metadata de grabacion VoIP en el puesto
 *  @param  on			true=ON/false=OFF
 *	@param	freqId		Identificador de la frecuencia 
 *	@param	resourceId  Identificador del recurso seleccionado en el bss
 *	@param	bssMethod	Método bss
 *	@param  bssQidx		Indice de calidad
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_RdSquEvent(bool on, const char *freqId, const char *resourceId, const char *bssMethod, unsigned int bssQidx, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		SipAgent::RdSquEvent(on, freqId, resourceId, bssMethod, bssQidx);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_RecorderCmd. Se pasan comandos para realizar acciones sobre el grabador VoIP
 *  @param  cmd			Comando
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_RecorderCmd(CORESIP_RecCmdType cmd, CORESIP_Error * error)
{
	int ret = CORESIP_OK;

	Try
	{
		ret = SipAgent::RecorderCmd(cmd, error);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_CreatePresenceSubscription. Crea una subscripcion por evento de presencia
 *  @param  dest_uri.	Uri del destino al que nos subscribimos
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreatePresenceSubscription(char *dest_uri, CORESIP_Error * error)
{
	int ret = CORESIP_OK;
	
	Try
	{
		ret = SipAgent::CreatePresenceSubscription(dest_uri);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_DestroyPresenceSubscription. destruye una subscripcion por evento de presencia
 *  @param  dest_uri.	Uri del destino al que nos desuscribimos
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_DestroyPresenceSubscription(char *dest_uri, CORESIP_Error * error)
{
	int ret = CORESIP_OK;
	
	Try
	{
		ret = SipAgent::DestroyPresenceSubscription(dest_uri);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_CreateConferenceSubscription. Crea una subscripcion por evento de conferencia
 *	@param	accId		Identificador del account.
 *  @param  dest_uri.	Uri del destino a monitorizar
 *  @param	by_proxy. Si true entonces el subscribe se envia a traves del proxy
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateConferenceSubscription(int accId, char *dest_uri, pj_bool_t by_proxy, CORESIP_Error * error)
{
	int ret = CORESIP_OK;
	
	Try
	{
		pj_assert((accId & CORESIP_ID_TYPE_MASK) == CORESIP_ACC_ID);
		ret = SipAgent::CreateConferenceSubscription(accId & CORESIP_ID_MASK, dest_uri, by_proxy);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_DestroyConferenceSubscription. Destruye una subscripcion por evento de conferencia
 *  @param  dest_uri.	Uri del destino a monitorizar
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_DestroyConferenceSubscription(char *dest_uri, CORESIP_Error * error)
{
	int ret = CORESIP_OK;
	
	Try
	{
		ret = SipAgent::DestroyConferenceSubscription(dest_uri);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_CreateDialogSubscription. Crea una subscripcion por evento de Dialogo
 *	@param	accId		Identificador del account.
 *  @param  dest_uri.	Uri del destino a monitorizar
 *  @param	by_proxy. Si true entonces el subscribe se envia a traves del proxy
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_CreateDialogSubscription(int accId, char *dest_uri, pj_bool_t by_proxy, CORESIP_Error * error)
{
	int ret = CORESIP_OK;
	
	Try
	{
		pj_assert((accId & CORESIP_ID_TYPE_MASK) == CORESIP_ACC_ID);
		ret = SipAgent::CreateDialogSubscription(accId & CORESIP_ID_MASK, dest_uri, by_proxy);
	}
	catch_all;

	return ret;
}

/**
 *	CORESIP_DestroyConferenceSubscription. Destruye una subscripcion por evento de Dialogo
 *  @param  dest_uri.	Uri del destino a monitorizar
 *	@param	error		Puntero a la Estructura de error
 *	@return				Codigo de Error
 */
CORESIP_API int CORESIP_DestroyDialogSubscription(char *dest_uri, CORESIP_Error * error)
{
	int ret = CORESIP_OK;
	
	Try
	{
		ret = SipAgent::DestroyDialogSubscription(dest_uri);
	}
	catch_all;

	return ret;
}

/**
 * SendInstantMessage. Envia un mensaje instantaneo
 *
 * @param	acc_id		Account ID to be used to send the request.
 * @param	dest_uri	Uri del destino del mensaje. Acabado en 0.
 * @param	text		Texto plano a enviar. Acabado en 0
 * @param	by_proxy	Si es true el mensaje se envia por el proxy
 * @return	Codigo de Error
 *
 */
CORESIP_API int CORESIP_SendInstantMessage(int acc_id, char *dest_uri, char *text, pj_bool_t by_proxy, CORESIP_Error * error)
{
	int ret = CORESIP_OK;
	
	Try
	{
		pj_assert((acc_id & CORESIP_ID_TYPE_MASK) == CORESIP_ACC_ID);
		ret = SipAgent::SendInstantMessage(acc_id & CORESIP_ID_MASK, dest_uri, text, by_proxy);
	}
	catch_all;

	return ret;
}

/**
 * CORESIP_EchoCancellerLCMic.	...
 * Activa/desactiva cancelador de eco altavoz LC y Microfonos. Sirve para el modo manos libres 
 * Por defecto está desactivado en la CORESIP
 * @param	on						true - activa / false - desactiva
 * @return	CORESIP_OK OK, CORESIP_ERROR  error.
 */
CORESIP_API int CORESIP_EchoCancellerLCMic(bool on, CORESIP_Error * error)
{
	int ret = CORESIP_OK;
	
	Try
	{
		ret = SipAgent::EchoCancellerLCMic(on);
	}
	catch_all;

	return ret;
}

CORESIP_API int CORESIP_SetImpairments(int call, CORESIP_Impairments * impairments, CORESIP_Error * error)
{
	int ret = CORESIP_OK;
	Try
	{
		pj_assert((call & CORESIP_ID_TYPE_MASK) == CORESIP_CALL_ID);
		SipCall::SetImpairments(call & CORESIP_ID_MASK, impairments);
	}
	catch_all;

	return ret;
}

/*@}*/