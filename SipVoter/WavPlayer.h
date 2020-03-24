#ifndef __CORESIP_WAVPLAYER_H__
#define __CORESIP_WAVPLAYER_H__

class WavPlayer
{
public:
	pjsua_conf_port_id Slot;

public:
	WavPlayer(const char * file, unsigned frameTime, bool loop, pj_status_t (*eofCb)(pjmedia_port *, void*), void * userData);
	~WavPlayer();

private:
	pj_pool_t * _Pool;
	pjmedia_port * _Port;
};

#endif
