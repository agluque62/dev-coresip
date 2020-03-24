#ifndef __CORESIP_RDRXPORT_H__
#define __CORESIP_RDRXPORT_H__

class RdRxPort : public pjmedia_port
{
public:
	pjsua_conf_port_id Slot;

public:
	RdRxPort(unsigned clkRate, unsigned channelCount, unsigned bitsPerSample, unsigned frameTime, 
		unsigned rClkRate, unsigned rChannelCount, unsigned rBitsPerSample, unsigned rFrameTime,
		const char * localIp, const char * mcastIp, unsigned mcastPort);
	~RdRxPort();

private:
	pj_pool_t * _Pool;
	pj_lock_t * _Lock;
	pjmedia_jbuf * _Jbuf;
	pjmedia_plc * _Plc;
	unsigned _RemoteSamplesPerFrame;
	char _LastFrameType;
	pj_sock_t _Sock;
	pj_activesock_t * _RemoteSock;

private:
	static pj_status_t GetFrame(pjmedia_port * port, pjmedia_frame * frame);
	static pj_status_t PutFrame(pjmedia_port * port, const pjmedia_frame * frame);
	static pj_status_t Reset(pjmedia_port * port);
	static pj_status_t Dispose(pjmedia_port * port);

	static pj_bool_t OnDataReceived(pj_activesock_t * asock, void * data, pj_size_t size, const pj_sockaddr_t *src_addr, int addr_len, pj_status_t status);
};

#endif
