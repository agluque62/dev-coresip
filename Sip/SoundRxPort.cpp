#include "Global.h"
#include "SoundRxPort.h"
#include "Exceptions.h"
#include "Guard.h"
#include "SipAgent.h"

SoundRxPort::SoundRxPort(const char * id, unsigned clkRate, unsigned channelCount, unsigned bitsPerSample, unsigned frameTime)
{
	pj_memset(this, 0, sizeof(SoundRxPort));

	_Pool = pjsua_pool_create(NULL, 1024, 512);
	pj_ansi_strcpy(Id, id);

	pj_status_t st = pj_lock_create_recursive_mutex(_Pool, NULL, &_Lock);
	PJ_CHECK_STATUS(st, ("ERROR creando seccion critica para puerto RdRx"));

	try
	{
		unsigned samplesPerFrame = clkRate * channelCount * frameTime / 1000;
		for (int i = 0; i < PJ_ARRAY_SIZE(_Ports); i++)
		{
			st = pjmedia_delay_buf_create(_Pool, NULL, clkRate, samplesPerFrame, channelCount, SipAgent::DefaultDelayBufPframes * frameTime, 0, &_SndInBufs[i]);
			PJ_CHECK_STATUS(st, ("ERROR creando SoundRxPort::_SndInfBuf"));

			pjmedia_port_info_init(&_Ports[i].info, &(pj_str("SNDP")), PJMEDIA_PORT_SIGNATURE('S', 'N', 'D', 'P'), 
				clkRate, channelCount, bitsPerSample, samplesPerFrame);

			_Ports[i].port_data.pdata = this;
			_Ports[i].port_data.ldata = i;
			_Ports[i].get_frame = &GetFrame;
			_Ports[i].reset = &Reset;

			st = pjsua_conf_add_port(_Pool, &_Ports[i], &Slots[i]);
			PJ_CHECK_STATUS(st, ("ERROR enlazando RdRx al mezclador"));
		}
	}
	catch (...)
	{
		Dispose();
		throw;
	}
}

SoundRxPort::~SoundRxPort()
{
	Dispose();
}

void SoundRxPort::Dispose()
{
	for (int i = 0; i < PJ_ARRAY_SIZE(_Ports); i++)
	{
		if (Slots[i] != PJSUA_INVALID_ID)
		{
			pjsua_conf_remove_port(Slots[i]);
		}
		if (_SndInBufs[i])
		{
			pjmedia_delay_buf_destroy(_SndInBufs[i]);
		}
	}

	if (_Lock)
	{
		pj_lock_destroy(_Lock);
	}
	if (_Pool)
	{
		pj_pool_release(_Pool);
	}
}

void SoundRxPort::PutFrame(CORESIP_SndDevType type, void * data, unsigned size)
{
	Guard lock(_Lock);
	pjmedia_delay_buf_put(_SndInBufs[type], (pj_int16_t*)data);
}

pj_status_t SoundRxPort::GetFrame(pjmedia_port * port, pjmedia_frame * frame)
{
	SoundRxPort * pThis = reinterpret_cast<SoundRxPort*>(port->port_data.pdata);
	Guard lock(pThis->_Lock);

	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	return pjmedia_delay_buf_get(pThis->_SndInBufs[port->port_data.ldata], (pj_int16_t*)frame->buf);
}

pj_status_t SoundRxPort::Reset(pjmedia_port * port)
{
	SoundRxPort * pThis = reinterpret_cast<SoundRxPort*>(port->port_data.pdata);
	Guard lock(pThis->_Lock);

	return pjmedia_delay_buf_reset(pThis->_SndInBufs[port->port_data.ldata]);
}
