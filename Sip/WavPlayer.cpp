#include "Global.h"
#include "WavPlayer.h"
#include "Exceptions.h"

WavPlayer::WavPlayer(const char * file, unsigned frameTime, bool loop, pj_status_t (*eofCb)(pjmedia_port *, void*), void * userData)
: _Pool(NULL), _Port(NULL)
{
	_Pool = pjsua_pool_create(NULL, 4096, 512);

	try
	{
		pj_status_t st = pjmedia_wav_player_port_create(_Pool, file, frameTime, loop ? 0 : PJMEDIA_FILE_NO_LOOP, 0, &_Port);
		PJ_CHECK_STATUS(st, ("ERROR creando WavPlayer", "[File=%s]", file));

		if (!loop)
		{
			pjmedia_wav_player_set_eof_cb(_Port, userData, eofCb);
		}

		st = pjsua_conf_add_port(_Pool, _Port, &Slot);
		PJ_CHECK_STATUS(st, ("ERROR enlazando WavPlayer al mezclador", "[File=%s]", file));
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

WavPlayer::~WavPlayer()
{
	pjsua_conf_remove_port(Slot);
	pjmedia_port_destroy(_Port);
	pj_pool_release(_Pool);
}
