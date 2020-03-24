/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2008 Intel Corporation. All Rights Reserved.
//
*/

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

#include <windows.h>

#if defined(_WIN32_WCE)
#include <winsock.h>
#endif /* defined(_WIN32_WCE) */

#include <stdio.h>
#include <ippcore.h>
#include <ipps.h>


#ifdef _UNICODE
# define vm_main wmain
#else
# define vm_main _tmain
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef __ICL

#define VM_ALIGN_DECL(X,Y) __declspec(align(X)) Y

#else /* !__ICL */

#define VM_ALIGN_DECL(X,Y) Y

#endif /* __ICL */

#define CONST_LL(X) X
#define CONST_ULL(X) X##ULL

/* vm_event.h */
typedef struct vm_event
{
    HANDLE handle;
} vm_event;

/* vm_mmap.h */
typedef struct vm_mmap
{
    HANDLE fd_file, fd_map;
    void  *address;
    Ipp32s fAccessAttr;
} vm_mmap;

/* vm_mutex.h */
typedef struct vm_mutex
{
    CRITICAL_SECTION sCritSection;
    Ipp32s iInited;
} vm_mutex;

/* vm_semaphore.h */
typedef struct vm_semaphore
{
    HANDLE handle;
} vm_semaphore;

/* vm_thread.h */
typedef struct vm_thread
{
    HANDLE handle;
    vm_mutex access_mut;
    Ipp32s i_wait_count;

    Ipp32u (__stdcall * protected_func)(void *);
    void *protected_arg;
} vm_thread;


typedef struct vm_time
{
   Ipp64s start;
   Ipp64s diff;
   Ipp64s freq;
} vm_time;

struct vm_timeval {
  Ipp32u tv_sec;
  long int tv_usec;
};

struct vm_timezone {
  int tz_minuteswest; // This is the number of minutes west of UTC.
  int tz_dsttime;    /* If nonzero, Daylight Saving Time applies
                     during some part of the year. */
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#pragma warning(disable: 4995) // "Warning: name was marked as #pragma deprecated"

#endif /* defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE) */
