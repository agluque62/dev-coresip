#ifndef __CORESIP_EXCEPTIONS_H__
#define __CORESIP_EXCEPTIONS_H__

#define PJ_CHECK_STATUS(st, info)\
	if (st != PJ_SUCCESS)\
		throw PJLibException(__FILE__, st).Msg info

class PJLibException
{
public:
	int Code;
	char File[CORESIP_MAX_FILE_PATH_LENGTH];
	char Info[CORESIP_MAX_ERROR_INFO_LENGTH];

public:
	PJLibException(const char * file, int st);
	PJLibException & Msg(const char * msg, const char * format = NULL, ...);

	int SetError(CORESIP_Error * error);
};

#endif
