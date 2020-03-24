#ifndef __CORESIP_GUARD_H__
#define __CORESIP_GUARD_H__

class Guard
{
public:
	Guard(pj_lock_t * lock) 
		: _Lock(lock), _Looked(false)
	{
		Lock();
	}

	~Guard()
	{
		Unlock();
	}

	void Lock()
	{
		pj_status_t st = pj_lock_acquire(_Lock);
		PJ_CHECK_STATUS(st, ("ERROR adquiriendo Lock"));
		_Looked = true;
	}

	void Unlock()
	{
		if (_Looked)
		{
			_Looked = false;
			pj_lock_release(_Lock);
		}
	}

private:
	pj_lock_t * _Lock;
	bool _Looked;
};

class DlgGuard
{
public:
	DlgGuard(pjsip_dialog * lock) 
		: _Lock(lock), _Looked(false)
	{
		Lock();
	}

	~DlgGuard()
	{
		Unlock();
	}

	void Lock()
	{
		pjsip_dlg_inc_lock(_Lock);
		_Looked = true;
	}

	void Unlock()
	{
		if (_Looked)
		{
			_Looked = false;
			pjsip_dlg_dec_lock(_Lock);
		}
	}

private:
	pjsip_dialog * _Lock;
	pj_bool_t _Looked;
};

#endif
