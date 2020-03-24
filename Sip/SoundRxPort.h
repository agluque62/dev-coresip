#ifndef __CORESIP_SOUNDRXPORT_H__
#define __CORESIP_SOUNDRXPORT_H__

class SoundRxPort
{
public:
	char Id[CORESIP_MAX_USER_ID_LENGTH + 1];
	pjsua_conf_port_id Slots[CORESIP_SND_MAX_IN_DEVICES];

public:
	SoundRxPort(const char * id, unsigned clkRate, unsigned channelCount, unsigned bitsPerSample, unsigned frameTime);
	~SoundRxPort();

	void PutFrame(CORESIP_SndDevType type, void * data, unsigned size);

private:
	pj_pool_t * _Pool;
	pjmedia_port _Ports[CORESIP_SND_MAX_IN_DEVICES];
	pjmedia_delay_buf * _SndInBufs[CORESIP_SND_MAX_IN_DEVICES];
	pj_lock_t * _Lock;

private:
	void Dispose();

	static pj_status_t GetFrame(pjmedia_port * port, pjmedia_frame * frame);
	static pj_status_t Reset(pjmedia_port * port);
};

#endif
