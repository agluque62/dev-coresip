#include "Global.h"
#include "WavPlayerToRemote.h"
#include "Exceptions.h"

#define __PJTHREAD__
// #define __PJCLOCK__

/** */
WavPlayerToRemote::WavPlayerToRemote(const char * file, unsigned frameTime, void (*eofCb)(void *))
{
	try
	{
		_Pool = pjsua_pool_create(NULL, 4096, 512);	

#ifdef __PJTHREAD__
		_thPool = pjsua_pool_create(NULL, 4096, 512);
#endif
#ifdef __PJCLOCK__
		_clkPool = pjsua_pool_create(NULL, 4096, 512);
#endif
	
		_RemoteSock = PJ_INVALID_SOCKET;
		pj_status_t st = pjmedia_wav_player_port_create(_Pool, file, frameTime, PJMEDIA_FILE_NO_LOOP, 0, &_Port);
		PJ_CHECK_STATUS(st, ("ERROR creando WavPlayer", "[File=%s]", file));
		
		_frameTime = frameTime;
		_eofCb = eofCb;
	}
	catch (...)
	{
		if (_Port)
		{
			pjmedia_port_destroy(_Port);
		}
		pj_pool_release(_Pool);

#ifdef __PJTHREAD__
		pj_pool_release(_thPool);
#endif
#ifdef __PJCLOCK__
		pj_pool_release(_clkPool);
#endif
		throw;
	}
}

/** */
WavPlayerToRemote::~WavPlayerToRemote(void)
{
	pjmedia_port_destroy(_Port);
	if (_RemoteSock != PJ_INVALID_SOCKET)
	{
		pj_sock_close(_RemoteSock);
	}

	// pjmedia_clock_destroy(_clock);

	pj_pool_release(_Pool);

#ifdef __PJTHREAD__
	pj_pool_release(_thPool);
#endif
#ifdef __PJCLOCK__
	pj_pool_release(_clkPool);
#endif
}

/** */
void WavPlayerToRemote::Send2Remote(const char * id, const char * ip, unsigned port)
{
	if (_RemoteSock == PJ_INVALID_SOCKET)
	{
		pj_ansi_strcpy(_RemotePayload.SrcId, id);

		_RemotePayload.SrcType = CORESIP_SND_INSTRUCTOR_MHP;		// Emulo al Instructor...

		pj_status_t st = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &_RemoteSock);	
		PJ_CHECK_STATUS(st, ("ERROR creando socket para el envio de radio por unicast"));
		
		pj_sockaddr_in_init(&_RemoteTo, &(pj_str(const_cast<char*>(ip))), (pj_uint16_t)port);

#ifdef __PJTHREAD__
		st = pj_thread_create(_thPool, "wav2rem", &Play, this, 0, 0, &thread); 
		pj_thread_set_prio(thread, pj_thread_get_prio_max(thread));
#endif
#ifdef __PJCLOCK__
		st = pjmedia_clock_create(_clkPool, 8000, 1, 320, 0, &TickPlay, this, &_clock);
		PJ_CHECK_STATUS(st, ("ERROR creando CLOCK para el envio WAV."));
		pjmedia_clock_start(_clock);		
#endif
	}
}

/** */
int WavPlayerToRemote::Play(void *proc)
{
	WavPlayerToRemote *wp = (WavPlayerToRemote *)proc;

	while (1) 
	{
		/** Espero PTIME */
		pj_thread_sleep(wp->_frameTime);
		if (wp->Tick()==FALSE)
		{
			// wp->_eofCb(wp);
			return 0;
		}
	}

	return 0;
}

/**  */
void WavPlayerToRemote::TickPlay(const pj_timestamp *ts, void *user_data)
{
	WavPlayerToRemote *wp = (WavPlayerToRemote *)user_data;
	if (wp->Tick()==FALSE)	
	{
		pjmedia_clock_stop(wp->_clock);
	}
}

/**  */
BOOL WavPlayerToRemote::Tick(void)
{
	frame.buf = samplebuf;
	frame.size = SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8);
	status = pjmedia_port_get_frame(_Port, &frame);

	if (status != PJ_SUCCESS || frame.type == PJMEDIA_FRAME_TYPE_NONE) 
	{
		return FALSE;
	}

	if (_RemoteSock != PJ_INVALID_SOCKET)
	{
		static pj_ssize_t size = sizeof(WavRemotePayload);

		_RemotePayload.Size = SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8);
		memcpy(_RemotePayload.Data, samplebuf, SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8));
		pj_sock_sendto(_RemoteSock, &_RemotePayload, &size, 0, &_RemoteTo, sizeof(_RemoteTo));
	}

	return TRUE;
}


