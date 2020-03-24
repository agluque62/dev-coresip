#include "Global.h"
#include "Exceptions.h"

PJLibException::PJLibException(const char * file, int st)
{
	Code = st;

	const char * ptr = file + pj_ansi_strlen(file);
	for (; (ptr != file) && (*ptr != '/') && (*ptr != '\\'); --ptr);
	ptr = (ptr == file ? file : ptr + 1);

	pj_ansi_strncpy(File, ptr, sizeof(File));
	File[sizeof(File) - 1] = 0;
}

PJLibException & PJLibException::Msg(const char * msg, const char * format, ...)
{
	int pos = 0;
	if (msg)
	{
		pos += pj_ansi_snprintf(Info, sizeof(Info), "%s: ", msg);
	}

	if (Code > 0)
	{
		char errmsg[PJ_ERR_MSG_SIZE];
		pj_strerror(Code, errmsg, sizeof(errmsg));
		pos += pj_ansi_snprintf(Info + pos, sizeof(Info) - pos, "%s ", errmsg);
	}

	if (format)
	{
		va_list arg;
		va_start(arg, format);
		pj_ansi_vsnprintf(Info + pos, sizeof(Info) - pos, format, arg);
		va_end(arg);
	}

	Info[sizeof(Info) - 1] = 0;

	return *this;
}

int PJLibException::SetError(CORESIP_Error * error)
{
	if (error)
	{
		error->Code = PJ_MIN(1, Code);
		pj_ansi_strcpy(error->File, File);
		pj_ansi_strcpy(error->Info, Info);
	}

	return Code;
}