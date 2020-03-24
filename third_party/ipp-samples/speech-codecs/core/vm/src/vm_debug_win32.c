/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2008 Intel Corporation. All Rights Reserved.
//
*/

#include <stdio.h>
#include <stdarg.h>
#include "vm_file.h"
#include "vm_debug.h"

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
# include <tchar.h>
# if defined(_WIN32_WCE)
#   include <Kfuncs.h>
# endif
#elif defined LINUX32
# include <time.h>
# include <sys/types.h>
# include <unistd.h>
# include <syslog.h>
#endif

#pragma warning(disable: 4100)

// here is line for global enable/disable debug trace
// and specify trace info
#if defined(_DEBUG)
#define VM_DEBUG (VM_DEBUG_FILE|VM_DEBUG_LOG_ALL|VM_DEBUG_DEFAULT_FORMAT)
#endif

#ifdef VM_DEBUG

#define VM_ONE_FILE_OPEN
#define VM_DEBUG_LINE   1024

static vm_debug_level  vm_DebugLevel = VM_DEBUG & VM_DEBUG_ALL;
static vm_debug_output vm_DebugOutput = VM_DEBUG_DISABLED;
static vm_char vm_LogFileName[_MAX_LEN] = VM_STRING("\\vm_debug_log.txt");
//static
vm_file *vm_LogFile = NULL;

//static vm_mutex mDebugMutex;
//static Ipp32s mDebugMutexIsValid = 1;

vm_debug_level vm_debug_setlevel(vm_debug_level new_level) {
    vm_debug_level old_level = vm_DebugLevel;
    vm_DebugLevel = new_level;
    return old_level;
}

vm_debug_output vm_debug_setoutput(vm_debug_output new_output) {
    vm_debug_output old_output = vm_DebugOutput;
    vm_DebugOutput = new_output;
    return old_output;
}

void vm_debug_setfile(vm_char *file, Ipp32s truncate)
{
    if (!file)
    {
        file = VM_STRING("");
    }

    if (!vm_string_strcmp(vm_LogFileName, file))
    {
        return;
    }

    if (vm_LogFile)
    {
        vm_file_fclose(vm_LogFile);
        vm_LogFile = NULL;
    }

    vm_string_strcpy(vm_LogFileName, file);
    if (*vm_LogFileName)
    {
        vm_LogFile = vm_file_fopen(vm_LogFileName, (truncate) ? VM_STRING("w") : VM_STRING("a"));
    }
}

void vm_debug_message(const vm_char *format,...)
{
    vm_char line[VM_DEBUG_LINE];
    va_list args;

    va_start(args, format);
    vm_string_vsprintf(line, format, args);
    va_end(args);
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
    MessageBox(GetActiveWindow(), line, VM_STRING("MESSAGE"), MB_OK);
#elif defined(LINUX32)
    fprintf(stderr, line);
#endif
}

void vm_debug_trace_ex(Ipp32s level,
                       const void *ptr_this,
                       const vm_char *func_name,
                       const vm_char *file_name,
                       Ipp32s num_line,
                       const vm_char *format,
                       ...)
{
    va_list args;
    vm_char tmp[VM_DEBUG_LINE] = {0,}, *p;

    if (vm_DebugOutput == VM_DEBUG_DISABLED) {
        return;
    }

    if (level && !(level & vm_DebugLevel)) {
        return;
    }

    //if (vm_string_strstr(file_name, "mpeg2_mux") && level == VM_DEBUG_PROGRESS) return;

    /* print filename and line */
    if (vm_DebugLevel & VM_DEBUG_SHOW_FILELINE) {
        int showMask;

        if (vm_DebugLevel &~ VM_DEBUG_SHOW_DIRECTORY) {
            p = vm_string_strrchr(file_name, '\\');
            if (p != NULL) file_name = p + 1;
            p = vm_string_strrchr(file_name, '/');
            if (p != NULL) file_name = p + 1;
        }
        vm_string_sprintf(tmp, VM_STRING("%s(%i), "), file_name, num_line);

        showMask = vm_DebugLevel & VM_DEBUG_SHOW_ALL;
        if (!(showMask &~ (VM_DEBUG_SHOW_FILELINE | VM_DEBUG_SHOW_FUNCTION))) {
          // in case of fileline&function only expand to 35 symbols
          p = tmp + vm_string_strlen(tmp);
          while (p < tmp + 32) *p++ = ' ';
          *p = 0;
        }
    }

    /* print time and PID */
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
    if (vm_DebugLevel & VM_DEBUG_SHOW_TIME) {
        SYSTEMTIME SystemTime;
        GetLocalTime(&SystemTime);

        vm_string_sprintf(tmp + vm_string_strlen(tmp),
                          VM_STRING("%u/%u/%u %u:%u:%u:%u, "),
                          SystemTime.wDay,
                          SystemTime.wMonth,
                          SystemTime.wYear,
                          SystemTime.wHour,
                          SystemTime.wMinute,
                          SystemTime.wSecond,
                          SystemTime.wMilliseconds);
    }
    if (vm_DebugLevel & VM_DEBUG_SHOW_PID) {
        vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("PID %i, "), GetCurrentProcessId());
    }
#elif defined(LINUX32)
    if (vm_DebugLevel & VM_DEBUG_SHOW_TIME) {
        time_t timep;
        struct tm *ptm;

        time(&timep);
        ptm = localtime((const time_t*) &timep);
        vm_string_sprintf(tmp + vm_string_strlen(tmp),
                          VM_STRING("%s%u/%u/%u %u:%u:%u, "),
                          ptm->tm_mday, 1+(ptm->tm_mon), 1900+(ptm->tm_year),
                          ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    }
    if (vm_DebugLevel & VM_DEBUG_SHOW_PID) {
        vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("%sPID %i, "), getpid());
    }
#endif

    /* print 'this' pointer if needed */
    if ((vm_DebugLevel & VM_DEBUG_SHOW_THIS) && ptr_this) {
        vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("%p, "), ptr_this);
    }

    /* print func_name if needed */
    if (func_name && (vm_DebugLevel & VM_DEBUG_SHOW_FUNCTION)) {
      p = vm_string_strstr(func_name, VM_STRING("::"));
      if (p) p += 2; else p = (vm_char *)func_name;
      vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("%s(): "), p);
    }

    if (vm_DebugLevel & VM_DEBUG_SHOW_LEVEL) {
      vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("Level%d, "), level);
    }

    va_start(args, format);
    vm_string_vsprintf(tmp + vm_string_strlen(tmp), format, args);
    va_end(args);

    // remove trailing \n
    p = tmp + vm_string_strlen(tmp) - 1;
    if (*p == 0xD || *p == 0xA) *p-- = 0;
    if (*p == 0xD || *p == 0xA) *p-- = 0;

    vm_string_strcat(tmp, VM_STRING("\n"));

    if (vm_DebugOutput & VM_DEBUG_FILE) {
        vm_file *pFile = NULL;
/*
        if (mDebugMutexIsValid) {
            vm_mutex_set_invalid(&mDebugMutex);
            res = vm_mutex_init(&mDebugMutex);
            if (res != VM_OK) {
                vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to init debug mutex !!!"));
                return;
            }
            mDebugMutexIsValid = 0;
        }
        res = vm_mutex_lock(&mDebugMutex);
        if (res != VM_OK) {
            vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to lock debug mutex !!!"));
            return;
        }
*/
        if (!vm_LogFile) {
          pFile =vm_file_fopen(vm_LogFileName, VM_STRING("w"));
          vm_LogFile = pFile;
        } else {
#ifdef VM_ONE_FILE_OPEN
          pFile = vm_LogFile;
#else
          pFile = fopen(vm_LogFileName, VM_STRING("a"));
#endif
        }
        if (pFile) {
#if defined(UNICODE) || defined(_UNICODE)
          fprintf(pFile, "%S", tmp);
#else
          vm_file_fprintf(pFile, "%s", tmp);
#endif
#ifdef VM_ONE_FILE_OPEN
          vm_file_fflush(pFile);
#else
          fclose(pFile);
#endif
        }
/*
        res = vm_mutex_unlock(&mDebugMutex);
        if (res != VM_OK) {
            vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to unlock debug mutex !!!"));
            return;
        }
*/
    }
    if (vm_DebugOutput & VM_DEBUG_CONSOLE) {
        vm_string_printf(tmp);
        fflush(stdout);
    }
    if (vm_DebugOutput & VM_DEBUG_SYSLOG) {
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
        OutputDebugString(tmp);
#elif defined(LINUX32)
        syslog("%s", tmp);
#endif
    }
}

static struct {
    int code;
    vm_char *string;
}
ErrorStringTable[] =
{
    { 0,                    VM_STRING("No any errors") },
    // VM
    { VM_OPERATION_FAILED,  VM_STRING("General operation fault") },
    { VM_NOT_INITIALIZED,   VM_STRING("VM_NOT_INITIALIZED") },
    { VM_TIMEOUT,           VM_STRING("VM_TIMEOUT") },
    { VM_NOT_ENOUGH_DATA,   VM_STRING("VM_NOT_ENOUGH_DATA") },
    { VM_NULL_PTR,          VM_STRING("VM_NULL_PTR") },
    { VM_SO_CANT_LOAD,      VM_STRING("VM_SO_CANT_LOAD") },
    { VM_SO_INVALID_HANDLE, VM_STRING("VM_SO_INVALID_HANDLE") },
    { VM_SO_CANT_GET_ADDR,  VM_STRING("VM_SO_CANT_GET_ADDR") },
    // UMC
    { -899,                 VM_STRING("UMC_ERR_INIT: Failed to initialize codec") },
    { -897,                 VM_STRING("UMC_ERR_SYNC: Required suncronization code was not found") },
    { -896,                 VM_STRING("UMC_ERR_NOT_ENOUGH_BUFFER: Buffer size is not enough") },
    { -895,                 VM_STRING("UMC_ERR_END_OF_STREAM: End of stream") },
    { -894,                 VM_STRING("UMC_ERR_OPEN_FAILED: Device/file open error") },
    { -883,                 VM_STRING("UMC_ERR_ALLOC: Failed to allocate memory") },
    { -882,                 VM_STRING("UMC_ERR_INVALID_STREAM: Invalid stream") },
    { -879,                 VM_STRING("UMC_ERR_UNSUPPORTED: Unsupported") },
    { -878,                 VM_STRING("UMC_ERR_NOT_IMPLEMENTED: Not implemented yet") },
    { -876,                 VM_STRING("UMC_ERR_INVALID_PARAMS: Incorrect parameters") },
    { 1,                    VM_STRING("UMC_WRN_INVALID_STREAM") },
    { 2,                    VM_STRING("UMC_WRN_REPOSITION_INPROGRESS") },
    { 3,                    VM_STRING("UMC_WRN_INFO_NOT_READY") },
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
    // Windows
    { 0x80004001L,          VM_STRING("E_NOTIMPL: Not implemented") },
    { 0x80004002L,          VM_STRING("E_NOINTERFACE: No such interface supported") },
    { 0x80004003L,          VM_STRING("E_POINTER: Invalid pointer") },
    { 0x80004004L,          VM_STRING("E_ABORT: Operation aborted") },
    { 0x80004005L,          VM_STRING("E_FAIL: Unspecified error") },
    { 0x8000FFFFL,          VM_STRING("E_UNEXPECTED: Catastrophic failure") },
    { 0x80070005L,          VM_STRING("E_ACCESSDENIED: General access denied error") },
    { 0x80070006L,          VM_STRING("E_HANDLE: Invalid handle") },
    { 0x8007000EL,          VM_STRING("E_OUTOFMEMORY: Ran out of memory") },
    { 0x80070057L,          VM_STRING("E_INVALIDARG: One or more arguments are invalid") },
    // DirectShow
    { 0x8004020BL,          VM_STRING("VFW_E_RUNTIME_ERROR: A run-time error occurred") },
    { 0x80040210L,          VM_STRING("VFW_E_BUFFERS_OUTSTANDING: One or more buffers are still active") },
    { 0x80040211L,          VM_STRING("VFW_E_NOT_COMMITTED: Cannot allocate a sample when the allocator is not active") },
    { 0x80040227L,          VM_STRING("VFW_E_WRONG_STATE: The operation could not be performed because the filter is in the wrong state") },
    // MediaFoundation
    { 0xC00D6D72L,          VM_STRING("MF_E_TRANSFORM_NEED_MORE_INPUT: The transform cannot produce output until it gets more input samples") },
#endif
};

vm_char *vm_get_error_string(int err_code)
{
    int i;
    for (i = 0; i < sizeof(ErrorStringTable)/sizeof(ErrorStringTable[0]); i++)
    {
        if (err_code == ErrorStringTable[i].code)
        {
            return ErrorStringTable[i].string;
        }
    }
    return VM_STRING("Unknown error");
}

long vm_trace_status(long err_code, vm_char *mess, void *ptr_this, vm_char *func, vm_char *file, int line)
{
    int level;
    vm_char *pString;
    if (!err_code)
    {
        level = VM_DEBUG_MACROS;
        pString = VM_STRING("No any errors");
    }
    else
    {
        level = VM_DEBUG_ERROR;
        pString = vm_get_error_string(err_code);
    }
    vm_debug_trace_ex(level, ptr_this, func, file, line, VM_STRING("%s = 0x%x = %s"), mess, err_code, pString);

    return err_code;
}

static const struct
{
    char *pName;
    UMC_GUID guid;
}
KnownGuids[] =
{
    "DXVA2_ModeWMV9_PostProc",      { 0x1b81be90, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5},
    "DXVA2_ModeWMV9_MoComp",        { 0x1b81be91, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5},
    "DXVA2_ModeWMV9_IDCT",          { 0x1b81be94, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5},
    "DXVA2_ModeMPEG2_MoComp",       { 0xe6a9f44b, 0x61b0, 0x4563, 0x9e,0xa4,0x63,0xd2,0xa3,0xc6,0xfe,0x66},
    "DXVA2_ModeMPEG2_IDCT",         { 0xbf22ad00, 0x03ea, 0x4690, 0x80,0x77,0x47,0x33,0x46,0x20,0x9b,0x7e},
    "DXVA2_ModeMPEG2_VLD",          { 0xee27417f, 0x5e28, 0x4e65, 0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9},
    "DXVA2_ModeH264_MoComp_NoFGT",  { 0x1b81be64, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5},
    "DXVA2_ModeH264_VLD_NoFGT",     { 0x1b81be68, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5},
    "DXVA2_ModeVC1_MoComp",         { 0x1b81beA1, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5},
    "DXVA2_ModeVC1_VLD",            { 0x1b81beA3, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5},
    "DXVA2_ModeMPEG4_VLD",          { 0x8ccf025a, 0xbacf, 0x4e44, 0x81,0x16,0x55,0x21,0xd9,0xb3,0x94,0x07},
    // MF formats and attributes
    "MFMediaType_Default",          { 0x81A412E6, 0x8103, 0x4B06, 0x85, 0x7F, 0x18, 0x62, 0x78, 0x10, 0x24, 0xAC},
    "MFMediaType_Audio",            { 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71},
    "MFMediaType_Video",            { 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71},
    "MF_MT_FRAME_SIZE",             { 0x1652c33d, 0xd6b2, 0x4012, 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d},
    "MF_MT_FRAME_RATE",             { 0xc459a2e8, 0x3d2c, 0x4e44, 0xb1, 0x32, 0xfe, 0xe5, 0x15, 0x6c, 0x7b, 0xb0},
    "MF_MT_PIXEL_ASPECT_RATIO",     { 0xc6376a1e, 0x8d0a, 0x4027, 0xbe, 0x45, 0x6d, 0x9a, 0x0a, 0xd3, 0x9b, 0xb6},
    "MF_MT_AVG_BITRATE",            { 0x20332624, 0xfb0d, 0x4d9e, 0xbd, 0x0d, 0xcb, 0xf6, 0x78, 0x6c, 0x10, 0x2e},
    "MF_MT_USER_DATA",              { 0xb6bc765f, 0x4c3b, 0x40a4, 0xbd, 0x51, 0x25, 0x35, 0xb6, 0x6f, 0xe0, 0x9d},
    "MF_MT_MAJOR_TYPE",             { 0x48eba18e, 0xf8c9, 0x4687, 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f},
    "MF_MT_SUBTYPE",                { 0xf7e34c9a, 0x42e8, 0x4714, 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5},
    "MF_MT_INTERLACE_MODE",         { 0xe2724bb8, 0xe676, 0x4806, 0xb4, 0xb2, 0xa8, 0xd6, 0xef, 0xb4, 0x4c, 0xcd},
    // DShow formats
    "FORMAT_None",                  { 0x0F6417D6, 0xc318, 0x11d0, 0xa4, 0x3f, 0x00, 0xa0, 0xc9, 0x22, 0x31, 0x96},
    "FORMAT_VideoInfo",             { 0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a},
    "FORMAT_VideoInfo2",            { 0xf72a76A0, 0xeb0a, 0x11d0, 0xac, 0xe4, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba},
    "FORMAT_WaveFormatEx",          { 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a},
    "FORMAT_MPEGVideo",             { 0x05589f82, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a},
    "FORMAT_MPEGStreams",           { 0x05589f83, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a},
    "FORMAT_DvInfo",                { 0x05589f84, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a},
    "FORMAT_525WSS",                { 0xc7ecf04d, 0x4582, 0x4869, 0x9a, 0xbb, 0xbf, 0xb5, 0x23, 0xb6, 0x2e, 0xdf},
};

void vm_trace_GUID_(const UMC_GUID *pGUID, vm_char *descr)
{
    int i;
    vm_char guid[256];

    for (i = 0; i < sizeof(KnownGuids)/sizeof(KnownGuids[0]); i++)
    {
        if (!memcmp(pGUID, &KnownGuids[i].guid, sizeof(UMC_GUID)))
        {
            vm_debug_trace2(VM_DEBUG_VERBOSE, VM_STRING("%s = ") VM_STRING_FORMAT, descr, KnownGuids[i].pName);
            return;
        }
    }

    vm_string_sprintf(guid,
        VM_STRING("%s = %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"),
        descr,
        pGUID->Data1, pGUID->Data2, pGUID->Data3,
        pGUID->Data4_0, pGUID->Data4_1, pGUID->Data4_2, pGUID->Data4_3,
        pGUID->Data4_4, pGUID->Data4_5, pGUID->Data4_6, pGUID->Data4_7);
    vm_debug_trace1(VM_DEBUG_VERBOSE, VM_STRING("%s"), guid);
}

#else /*  !VM_DEBUG */

vm_debug_level vm_debug_setlevel(vm_debug_level level)
{
    return level;
}

vm_debug_output vm_debug_setoutput(vm_debug_output output)
{
    return output;
}

void vm_debug_setfile(vm_char *file, Ipp32s truncate) {}

void vm_debug_message(const vm_char *format, ...) {}

void vm_debug_trace_ex(Ipp32s level, const vm_char *func_name, const vm_char *file_name,
                       Ipp32s num_line, const vm_char *format, ...){}

long vm_trace_status(long err_code, vm_char *mess, void *ptr_this, vm_char *func, vm_char *file, int line);

void vm_trace_GUID_(const UMC_GUID *pGUID, vm_char *descr) {}

vm_char *vm_get_error_string(int err_code) { return VM_STRING("n/a due to release build"); }

#endif /* VM_DEBUG */
