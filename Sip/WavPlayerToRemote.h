#pragma once

/** */
struct WavRemotePayload
{
	char SrcId[CORESIP_MAX_USER_ID_LENGTH + 1];
	CORESIP_SndDevType SrcType;
	unsigned Size;
	char Data[SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8)];
};

class WavPlayerToRemote
{
public:
	WavPlayerToRemote(const char * file, unsigned frameTime, void (*eofCb)(void *));
	~WavPlayerToRemote(void);

	void Send2Remote(const char * id, const char * ip, unsigned port);
	BOOL Tick(void );

private:
	static pj_thread_proc Play;
	static pjmedia_clock_callback TickPlay;

private:
	unsigned _frameTime;
	void (*_eofCb)(void *);

private:
	pj_pool_t * _Pool;
	pj_pool_t * _thPool;
	pj_pool_t *_clkPool;

	pjmedia_port * _Port;

	pj_thread_t  *thread;
	pjmedia_clock *_clock;

    HANDLE ptrTimerHandle;			/** Temporizador de Windows... */


	pj_sock_t _RemoteSock;
	pj_sockaddr_in _RemoteTo;
	WavRemotePayload _RemotePayload;

	pj_int16_t samplebuf[SAMPLES_PER_FRAME * (BITS_PER_SAMPLE / 8) + 32];
	pjmedia_frame frame;	
	pj_status_t status;
};

