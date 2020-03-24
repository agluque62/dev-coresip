/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2008 Intel Corporation. All Rights Reserved.
//
*/
#include "vm_time.h"

#define VM_TIMEOP(A,B,C,OP) A = vvalue(B) OP vvalue(C)

#define VM_TIMEDEST \
  destination[0].tv_sec = (Ipp32u)(cv0 / 1000000); \
  destination[0].tv_usec = (long)(cv0 % 1000000)

static Ipp64u vvalue( struct vm_timeval* B )
{
  Ipp64u rtv;
  rtv = B[0].tv_sec;
  return ((rtv * 1000000) + B[0].tv_usec);
}


/* common (Linux and Windows time functions
 * may be placed before compilation fence. */
void vm_time_timeradd(struct vm_timeval* destination,  struct vm_timeval* src1, struct vm_timeval* src2)
{
  Ipp64u cv0;
  VM_TIMEOP(cv0, src1, src2, + );
  VM_TIMEDEST;
}

void vm_time_timersub(struct vm_timeval* destination,  struct vm_timeval* src1, struct vm_timeval* src2)
{
  Ipp64u cv0;
  VM_TIMEOP(cv0, src1, src2, - );
  VM_TIMEDEST;
}
//
// returning value:
//   0 - equal
//  -1 - src1 less than src2
//   1 - src1 more than src2
int vm_time_timercmp(struct vm_timeval* src1, struct vm_timeval* src2, struct vm_timeval *threshold)
{
  Ipp64u val1 = vvalue(src1);
  Ipp64u val2 = vvalue(src2);
  int rtval = 0;
  if ( val1 != val2 ) {
  Ipp64u thr  = vvalue(threshold);
  if (thr != 0) {
    val2 = thr;
    val1 = (val1 > val2 ) ? val1 - val2 : val2 - val1;
    }
  rtval = (val1 < val2) ? -1 : 1;
  }
  return rtval;
}

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

#include "time.h"
#include "winbase.h"


/* yield the execution of current thread for msec milliseconds */
void vm_time_sleep(Ipp32u msec)
{
#ifndef _WIN32_WCE
  if (msec) {
    Sleep(msec);
    }
  else
    SwitchToThread();
#else
  Sleep(msec);  /* always Sleep for WinCE */
#endif

} /* void vm_time_sleep(Ipp32u msec)  */

/* obtain the clock tick of an uninterrupted master clock */
vm_tick vm_time_get_tick(void)
{
    LARGE_INTEGER t1;

    QueryPerformanceCounter(&t1);
    return t1.QuadPart;

} /* vm_tick vm_time_get_tick(void) */

/* obtain the clock resolution */
vm_tick vm_time_get_frequency(void)
{
    LARGE_INTEGER t1;

    QueryPerformanceFrequency(&t1);
    return t1.QuadPart;

} /* vm_tick vm_time_get_frequency(void) */

/* Create the object of time measure */
vm_status vm_time_open(vm_time_handle *handle)
{
   if (NULL == handle)
       return VM_NULL_PTR;
   *handle = -1;
   return VM_OK;

} /* vm_status vm_time_open(vm_time_handle *handle); */

/* Close the object of time measure */
vm_status vm_time_close(vm_time_handle *handle)
{
   if (NULL == handle)
       return VM_NULL_PTR;
   *handle = -1;
   return VM_OK;

} /* vm_status vm_time_close(vm_time_handle *handle) */

/* Initialize the object of time measure */
vm_status vm_time_init(vm_time *m)
{
   if (NULL == m)
       return VM_NULL_PTR;
   m->start = 0;
   m->diff = 0;
   m->freq = vm_time_get_frequency();
   return VM_OK;

} /* vm_status vm_time_init(vm_time *m) */

/* Start the process of time measure */
vm_status vm_time_start(vm_time_handle handle, vm_time *m)
{
    /*  touch unreferenced parameters.
        Take into account Intel's compiler. */
    handle = handle;

   if (NULL == m)
       return VM_NULL_PTR;
   m->start = vm_time_get_tick();
   return VM_OK;

} /* vm_status vm_time_start(vm_time_handle handle, vm_time *m) */

/* Stop the process of time measure and obtain the sampling time in seconds */
Ipp64f vm_time_stop(vm_time_handle handle, vm_time *m)
{
   Ipp64f speed_sec;
   vm_tick end;

   /*  touch unreferenced parameters.
       Take into account Intel's compiler. */
   handle = handle;

   end = vm_time_get_tick();
   m->diff += end - m->start;

   if(m->freq == 0) m->freq = vm_time_get_frequency();
   speed_sec = (Ipp64f)m->diff / (Ipp64f)m->freq;
   return speed_sec;

} /* Ipp64f vm_time_stop(vm_time_handle handle, vm_time *m) */

static Ipp64u offset_from_1601_to_1970 = 0;
vm_status vm_time_gettimeofday( struct vm_timeval *TP, struct vm_timezone *TZP ) {
  /* FILETIME data structure is a 64-bit value representing the number
               of 100-nanosecond intervals since January 1, 1601 */
  Ipp64u tmb;
  SYSTEMTIME bp;
  if ( offset_from_1601_to_1970 == 0 ) {
    /* prepare 1970 "epoch" offset */
    bp.wDay = 1; bp.wDayOfWeek = 4; bp.wHour = 0;
    bp.wMinute = 0; bp.wMilliseconds = 0;
    bp.wMonth = 1; bp.wSecond = 0;
    bp.wYear = 1970;
    SystemTimeToFileTime(&bp, (FILETIME *)&offset_from_1601_to_1970);
  }
#ifndef _WIN32_WCE
  GetSystemTimeAsFileTime((FILETIME *)&tmb);
#else
  GetSystemTime(&bp);
  SystemTimeToFileTime(&bp, (FILETIME *)&tmb);
#endif
  tmb -= offset_from_1601_to_1970; /* 100 nsec ticks since 1 jan 1970 */
  TP[0].tv_sec = (Ipp32u)(tmb / 10000000); /* */
  TP[0].tv_usec = (long)((tmb % 10000000) / 10); /* microseconds OK? */
  return  (TZP != NULL) ? VM_NOT_INITIALIZED : VM_OK;
}

#endif /* defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE) */
