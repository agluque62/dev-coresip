#include "Global.h"
#include "WavRecorder.h"
#include "Exceptions.h"

WavRecorder::WavRecorder(const char * file)
: _Pool(NULL), _Port(NULL)
{
	_Pool = pjsua_pool_create(NULL, 4096, 512);

	try
	{
		pj_status_t st = pjmedia_wav_writer_port_create(_Pool, file, SAMPLING_RATE, CHANNEL_COUNT, SAMPLES_PER_FRAME, BITS_PER_SAMPLE,
			PJMEDIA_FILE_WRITE_PCM, 0, &_Port);
		PJ_CHECK_STATUS(st, ("ERROR creando WavRecorder", "[File=%s]", file));

		st = pjsua_conf_add_port(_Pool, _Port, &Slot);
		PJ_CHECK_STATUS(st, ("ERROR enlazando WavRecorder al mezclador", "[File=%s]", file));
	}
	catch (...)
	{
		if (_Port)
		{
			pjmedia_port_destroy(_Port);
		}
		pj_pool_release(_Pool);

		throw;
	}
}

WavRecorder::~WavRecorder()
{
	pjsua_conf_remove_port(Slot);
	pjmedia_port_destroy(_Port);
	pj_pool_release(_Pool);
}
