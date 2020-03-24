/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2008 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __VM_SYS_INFO_H__
#define __VM_SYS_INFO_H__


#include "vm_types.h"
#include "vm_time.h"
#define OLDWINNT 0
#if defined(_WIN32) || defined(_WIN64)
# if _MSC_VER >= 1400
#  pragma warning(disable: 4996)
# endif

/* for performance monitoring */
# if _WIN32_WINNT < 0x501
# undef OLDWINNT
# define OLDWINNT _WIN32_WINNT
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x501
# endif
# include <pdh.h>
# include <pdhmsg.h>
#endif // if defined(_WIN32) || defined(_WIN64)

#undef UMC_VERSION_INFO
#ifdef UMC_VERSION_INFO
/*
 * umc_version.gen file contains
 *     vm_char *umc_version_string = "Current UMC version";
 *     Ipp64u   umc_version_number = 0xVERSION_NUMBER;
 */
#  include "umc_version.gen"
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Processor's feature bits */
enum
{
    CMOV_FEATURE                = 0x00001,
    MMX_FEATURE                 = 0x00002,
    MMX_EXT_FEATURE             = 0x00004,
    SSE_FEATURE                 = 0x00008,
    SSE2_FEATURE                = 0x00010,
    SSE3_FEATURE                = 0x00020,

    UNK_MANUFACTURE             = 0x00000,
    INTEL_MANUFACTURE           = 0x10000
};

/* Processors ID list */
enum
{
    UNKNOWN_CPU                 = 0,

    PENTIUM                     = INTEL_MANUFACTURE,
    PENTIUM_PRO                 = PENTIUM | CMOV_FEATURE,
    PENTIUM_MMX                 = PENTIUM | MMX_FEATURE,
    PENTIUM_2                   = PENTIUM_MMX | CMOV_FEATURE,
    PENTIUM_3                   = PENTIUM_2 | MMX_EXT_FEATURE | SSE_FEATURE,
    PENTIUM_4                   = PENTIUM_3 | SSE2_FEATURE,
    PENTIUM_4_PRESCOTT          = PENTIUM_4 | SSE3_FEATURE
};

typedef enum
{
    DDMMYY = 0,
    MMDDYY = 1,
    YYMMDD = 2
} DateFormat;

typedef enum
{
    HHMMSS      = 0,
    HHMM        = 1,
    HHMMSSMS1   = 2,
    HHMMSSMS2   = 3,
    HHMMSSMS3   = 4
} TimeFormat;

#ifdef UMC_VERSION_INFO
    /* Functions to obtain UMC version information */
    vm_char *vm_get_version_string( void );
    Ipp64u vm_get_version_number( void );
#endif

/*
 * structures to obtain processor loading info
 */
struct VM_SYSINFO_CPULOAD_ENTRY {
  /* all are floats 0. - 1. */
  float usrload;
  float sysload;
  float idleload;
  /* unixes specific info - may be useful */
  float usrniceload;
  float iowaitsload;
  float irqsrvload;
  float softirqsrvload;
  float vmstalled;  /* time to sleep due to processor "occupation" by another OS under Xen */
  };

# if defined(_WIN32) || defined(_WIN64)
#  define MAX_PERF_PARAMETERS 3
# endif

struct VM_SYSINFO_CPULOAD {
  Ipp32s  ncpu;
  vm_tick  tickspassed;
  struct VM_SYSINFO_CPULOAD_ENTRY *cpudes;
# if defined(_WIN32) || defined(_WIN64)
  /* additional fields to hold system specific info for Windows */
  HQUERY CpuLoadQuery;
  PDH_HCOUNTER *CpuPerfCounters[MAX_PERF_PARAMETERS];   /* we will receive only 3 parameters:
                                   user time, system time and interrupt time */
# endif
  };

/* Functions to obtain processor's specific information */
void vm_sys_info_get_cpu_name(vm_char *cpu_name);
void vm_sys_info_get_date(vm_char *m_date, DateFormat df);
void vm_sys_info_get_time(vm_char *m_time, TimeFormat tf);
void vm_sys_info_get_vga_card(vm_char *vga_card);
void vm_sys_info_get_os_name(vm_char *os_name);
void vm_sys_info_get_computer_name(vm_char *computer_name);
void vm_sys_info_get_program_name(vm_char *program_name);
void vm_sys_info_get_program_path(vm_char *program_path);
void vm_sys_info_get_program_description(vm_char *program_description);
Ipp32u vm_sys_info_get_cpu_speed(void);
Ipp32u vm_sys_info_get_mem_size(void);
Ipp32u vm_sys_info_get_avail_cpu_num(void); 
Ipp32u vm_sys_info_get_cpu_num(void); 

/*
 * CPU loading information functions are not available for Linux and 
 * other OSes yet 
 */
# if defined(_WIN32) || defined(_WIN64)
/*
 * CPU loading information obtaining functions
 */
struct VM_SYSINFO_CPULOAD *vm_sys_info_create_cpu_data(void);
vm_status vm_sys_info_cpu_loading_avg(struct VM_SYSINFO_CPULOAD *dbuf, Ipp32u timeslice);
vm_status vm_sys_info_remove_cpu_data(struct VM_SYSINFO_CPULOAD *dbuf);

#if OLDWINNT > 0
# undef _WIN32_WINNT
# define _WIN32_WINNT OLDWINNT
# undef OLDWINNT
# define OLDWINNT 0
#endif

# endif /* defined(_WIN32) || defined(_WIN64) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VM_SYS_INFO_H__ */
