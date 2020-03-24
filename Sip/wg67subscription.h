#ifndef __CORESIP_WG67_SUBSCRIPTION_H__
#define __CORESIP_WG67_SUBSCRIPTION_H__

class WG67Subscription
{
public:
	static void Init(pj_pool_t * pool, const CORESIP_Config * cfg);
	static void End();

private:
	static CORESIP_Callbacks _Cb;
	static pjsip_evsub_user wg67_callback; 

public:
	WG67Subscription(const char * dst);
	~WG67Subscription();

private:
	pj_pool_t * _Pool;
	pj_str_t _Dst;
	pjsip_evsub * _Module;

private:

	static void wg67_on_state(pjsip_evsub *sub, pjsip_event *event);
	static void wg67_on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body);
};

#endif
