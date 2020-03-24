#ifndef __CORESIP_GLOBAL_H__
#define __CORESIP_GLOBAL_H__

#ifdef _WIN32
#include <winsock.h>
#endif

#ifdef _ED137_
#include <pjsip-simple/evsub_msg.h>
#include <pjsip-simple/confsub.h>
#include <pjmedia-audiodev/audiodev_imp.h>
#endif

#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>

#include "CoreSip.h"

#define SAMPLING_RATE			8000
#define CHANNEL_COUNT			1
#define PTIME					20
#define SAMPLES_PER_FRAME		((SAMPLING_RATE * CHANNEL_COUNT * PTIME) / 1000)
#define BITS_PER_SAMPLE			16

#define SND_SAMPLING_RATE		8000

#define DELAY_BUF_PFRAMES		4

#define RESTART_JBUF			((char)0x52)

#endif
