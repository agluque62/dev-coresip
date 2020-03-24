#include "Global.h"
#include "RdRxPort.h"
#include "Exceptions.h"
#include "Guard.h"

RdRxPort::RdRxPort(unsigned clkRate, unsigned channelCount, unsigned bitsPerSample, unsigned frameTime, 
							unsigned rClkRate, unsigned rChannelCount, unsigned rBitsPerSample, unsigned rFrameTime,
							const char * localIp, const char * mcastIp, unsigned mcastPort)
{
	pj_memset(this, 0, sizeof(RdRxPort));

	_Pool = pjsua_pool_create(NULL, 512, 512);
	_RemoteSamplesPerFrame = rClkRate * rChannelCount * rFrameTime / 1000;
	_Sock = PJ_INVALID_SOCKET;

	unsigned samplesPerFrame = clkRate * channelCount * frameTime / 1000;
	pjmedia_port_info_init(&info, &(pj_str("RSTR")), PJMEDIA_PORT_SIGNATURE('R', 'S', 'T', 'R'), 
		clkRate, channelCount, bitsPerSample, samplesPerFrame);

	port_data.pdata = this;

	get_frame = &GetFrame;
	put_frame = &PutFrame;
	reset = &Reset;
	on_destroy = &Dispose;

	try
	{
		pj_status_t st = pj_lock_create_recursive_mutex(_Pool, NULL, &_Lock);
		PJ_CHECK_STATUS(st, ("ERROR creando seccion critica para puerto de recepcion multicast radio"));

		unsigned jb_max = (pjsua_var.media_cfg.jb_max >= (int)rFrameTime) ? 
			((pjsua_var.media_cfg.jb_max + rFrameTime - 1) / rFrameTime) : gJBufPframes; //(500 / rFrameTime);
		unsigned jb_min_pre = (pjsua_var.media_cfg.jb_min_pre >= (int)rFrameTime) ? 
			(pjsua_var.media_cfg.jb_min_pre / rFrameTime) : 1;
		unsigned jb_max_pre = (pjsua_var.media_cfg.jb_max_pre >= (int)rFrameTime) ? 
			(pjsua_var.media_cfg.jb_max_pre / rFrameTime) : (jb_max * 4 / 5);
		unsigned jb_init = (pjsua_var.media_cfg.jb_init >= (int)rFrameTime) ? 
			(pjsua_var.media_cfg.jb_init / rFrameTime) : 0;

		st = pjmedia_jbuf_create(_Pool, &(pj_str("RSTR")), _RemoteSamplesPerFrame * (rBitsPerSample / 8), rFrameTime, jb_max, &_Jbuf);
		PJ_CHECK_STATUS(st, ("ERROR creando buffer jitter para puerto de recepcion multicast radio"));
		pjmedia_jbuf_set_adaptive(_Jbuf, jb_init, jb_min_pre, jb_max_pre);

		st = pjmedia_plc_create(_Pool, rClkRate, _RemoteSamplesPerFrame, 0, &_Plc);
		PJ_CHECK_STATUS(st, ("ERROR creando plc para puerto de recepcion multicast radio"));

		pj_sockaddr_in addr, mcastAddr;
		pj_sockaddr_in_init(&addr, &(pj_str(const_cast<char*>(localIp))), (pj_uint16_t)mcastPort);
		pj_sockaddr_in_init(&mcastAddr, &(pj_str(const_cast<char*>(mcastIp))), (pj_uint16_t)mcastPort);

		st = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &_Sock);
		PJ_CHECK_STATUS(st, ("ERROR creando socket para puerto de recepcion multicast radio"));

		int on = 1;
		pj_sock_setsockopt(_Sock, pj_SOL_SOCKET(), SO_REUSEADDR, (void *)&on, sizeof(on));

		st = pj_sock_bind(_Sock, &addr, sizeof(addr));
		PJ_CHECK_STATUS(st, ("ERROR enlazando socket para puerto de recepcion multicast radio", "[Ip=%s][Port=%d]", localIp, mcastPort));

		struct ip_mreq	mreq;
		mreq.imr_multiaddr.S_un.S_addr = mcastAddr.sin_addr.s_addr;
		mreq.imr_interface.S_un.S_addr = addr.sin_addr.s_addr;

		st = pj_sock_setsockopt(_Sock, IPPROTO_IP, pj_IP_ADD_MEMBERSHIP(), (void *)&mreq, sizeof(mreq));
		PJ_CHECK_STATUS(st, ("ERROR añadiendo socket a multicast para puerto de recepcion multicast radio", "[Mcast=%s][Port=%d]", mcastIp, mcastPort));

		pj_activesock_cb cb = { NULL };
		cb.on_data_recvfrom = &OnDataReceived;

		st = pj_activesock_create(_Pool, _Sock, pj_SOCK_DGRAM(), NULL, pjsip_endpt_get_ioqueue(pjsua_get_pjsip_endpt()), &cb, this, &_RemoteSock);
		PJ_CHECK_STATUS(st, ("ERROR creando servidor de lectura para puerto de recepcion multicast radio"));

		st = pj_activesock_start_recvfrom(_RemoteSock, _Pool, (_RemoteSamplesPerFrame * (rBitsPerSample / 8)) + sizeof(unsigned), 0);
		PJ_CHECK_STATUS(st, ("ERROR iniciando lectura en puerto de recepcion multicast radio"));

		st = pjsua_conf_add_port(_Pool, this, &Slot);
		PJ_CHECK_STATUS(st, ("ERROR añadiendo al mezclador el puerto de recepcion multicast radio"));
	}
	catch (...)
	{
		Dispose(this);
		throw;
	}
}

RdRxPort::~RdRxPort()
{
	pjsua_conf_remove_port(Slot);
	Dispose(this);
}

pj_status_t RdRxPort::GetFrame(pjmedia_port * port, pjmedia_frame * frame)
{
	RdRxPort * pThis = reinterpret_cast<RdRxPort*>(port->port_data.pdata);
	unsigned samples_required = port->info.samples_per_frame;
	unsigned samples_per_frame = pThis->_RemoteSamplesPerFrame;
	pj_int16_t * p_out_samp = (pj_int16_t*)frame->buf;

	Guard lock(pThis->_Lock);

	for (unsigned samples_count = 0; samples_count < samples_required; samples_count += samples_per_frame) 
	{
		char frame_type;
		pj_size_t frame_size;
		pj_uint32_t bit_info;

		assert((frame->size - (samples_count * 2)) >= (samples_per_frame * 2));
		pjmedia_jbuf_get_frame2(pThis->_Jbuf, p_out_samp + samples_count, &frame_size, &frame_type, &bit_info);

		if (frame_type == PJMEDIA_JB_MISSING_FRAME) 
		{
			pj_status_t st = pjmedia_plc_generate(pThis->_Plc, p_out_samp + samples_count);

			if (st != PJ_SUCCESS) 
			{
				pjmedia_zero_samples(p_out_samp + samples_count, samples_required - samples_count);
				PJ_LOG(5, (port->info.name.ptr,  "Frame lost!"));
			} 
			else 
			{
				PJ_LOG(5, (port->info.name.ptr, "Lost frame recovered"));
			}
		} 
		else if (frame_type == PJMEDIA_JB_ZERO_EMPTY_FRAME) 
		{
			/* Jitter buffer is empty. If this is the first "empty" state,
			* activate PLC to smoothen the fade-out, otherwise zero
			* the frame. 
			*/
			if (frame_type != pThis->_LastFrameType) 
			{
				do 
				{
					pj_status_t st = pjmedia_plc_generate(pThis->_Plc, p_out_samp + samples_count);
					if (st != PJ_SUCCESS)
					{
						break;
					}

					samples_count += samples_per_frame;

				} while (samples_count < samples_required);
			}

			if (samples_count < samples_required) 
			{
				pjmedia_zero_samples(p_out_samp + samples_count, samples_required - samples_count);
				samples_count = samples_required;
			}

			pThis->_LastFrameType = frame_type;
			break;
		} 
		else if (frame_type != PJMEDIA_JB_NORMAL_FRAME) 
		{
			/* It can only be PJMEDIA_JB_ZERO_PREFETCH frame */
			pj_assert(frame_type == PJMEDIA_JB_ZERO_PREFETCH_FRAME);

			do 
			{
				pj_status_t st = pjmedia_plc_generate(pThis->_Plc, p_out_samp + samples_count);
				if (st != PJ_SUCCESS)
				{
					break;
				}

				samples_count += samples_per_frame;

			} while (samples_count < samples_required);

			if (samples_count < samples_required) 
			{
				pjmedia_zero_samples(p_out_samp + samples_count, samples_required - samples_count);
				samples_count = samples_required;
			}

			pThis->_LastFrameType = frame_type;
			break;
		} 
		else 
		{
			pjmedia_plc_save(pThis->_Plc, p_out_samp + samples_count);
		}

		pThis->_LastFrameType = frame_type;
	}

	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	return PJ_SUCCESS;
}

pj_status_t RdRxPort::PutFrame(pjmedia_port * port, const pjmedia_frame * frame)
{
	return PJ_SUCCESS;
}

pj_status_t RdRxPort::Reset(pjmedia_port * port)
{
	RdRxPort * pThis = reinterpret_cast<RdRxPort*>(port->port_data.pdata);
	Guard lock(pThis->_Lock);

	return pjmedia_jbuf_reset(pThis->_Jbuf);
}

pj_status_t RdRxPort::Dispose(pjmedia_port * port) 
{
	RdRxPort * pThis = reinterpret_cast<RdRxPort*>(port->port_data.pdata);

	if (pThis->_RemoteSock)
	{
		pj_activesock_close(pThis->_RemoteSock);
	}
	else if (pThis->_Sock != PJ_INVALID_SOCKET)
	{
		pj_sock_close(pThis->_Sock);
	}
	//if (pThis->_Plc)
	//{
	//}
	if (pThis->_Jbuf)
	{
		pjmedia_jbuf_destroy(pThis->_Jbuf);
	}
	if (pThis->_Lock)
	{
		pj_lock_destroy(pThis->_Lock);
	}
	if (pThis->_Pool)
	{
		pj_pool_release(pThis->_Pool);
	}

	return PJ_SUCCESS;
}

pj_bool_t RdRxPort::OnDataReceived(pj_activesock_t * asock, void * data, pj_size_t size, const pj_sockaddr_t *src_addr, int addr_len, pj_status_t status)
{
	RdRxPort * pThis = (RdRxPort*)pj_activesock_get_user_data(asock);

	if (status == PJ_SUCCESS)
	{
		if (size == ((pThis->_RemoteSamplesPerFrame * (BITS_PER_SAMPLE / 8)) + sizeof(unsigned)))
		{
			unsigned ext_seq = *((unsigned*)((char*)data + size - sizeof(unsigned)));

			Guard lock(pThis->_Lock);
			pjmedia_jbuf_put_frame2(pThis->_Jbuf, data, size - sizeof(unsigned), 0, (int)pj_ntohl(ext_seq), NULL);
		}
		else if (size == 1)
		{
			if (*((char*)data) == RESTART_JBUF)
			{
				Guard lock(pThis->_Lock);
				pjmedia_jbuf_reset(pThis->_Jbuf);
			}
		}
	}

	return PJ_TRUE;
}