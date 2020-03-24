#ifndef __CORESIP_WAVRECORDER_H__
#define __CORESIP_WAVRECORDER_H__

class WavRecorder
{
public:
	pjsua_conf_port_id Slot;

public:
	WavRecorder(const char * file);
	~WavRecorder();

private:
	pj_pool_t * _Pool;
	pjmedia_port * _Port;
};

#endif
