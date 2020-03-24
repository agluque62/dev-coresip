/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2007 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __VM_STRINGS_H__
#define __VM_STRINGS_H__

#ifndef __IPPDEFS_H__
#include "ippdefs.h"
#endif

#ifdef LINUX32

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include <dirent.h>

typedef char vm_char;

# include "vm_file.h"


#define VM_STRING(x) x

#define vm_string_printf    printf
#define vm_string_fprintf   vm_file_fprintf
#define vm_string_sprintf   sprintf
#define vm_string_snprintf  snprintf
#define vm_string_vprintf   vprintf
#define vm_string_vfprintf  vm_file_vfprintf
#define vm_string_vsprintf  vsprintf

#define vm_string_strcat    strcat
#define vm_string_strncat   strncat
#define vm_string_strcpy    strcpy
#define vm_string_strncpy   strncpy
#define vm_string_strcspn   strcspn
#define vm_string_strspn    strspn

#define vm_string_strlen    strlen
#define vm_string_strcmp    strcmp
#define vm_string_strncmp   strncmp
#define vm_string_stricmp   strcasecmp
#define vm_string_strnicmp  strncasecmp
#define vm_string_strrchr   strrchr

#define vm_string_atol      atol
#define vm_string_atoi      atoi
#define vm_string_atof      atof

#define vm_string_strstr    strstr
#define vm_string_sscanf    sscanf
#define vm_string_strchr    strchr

#define vm_finddata_t struct _finddata_t
#define vm_string_splitpath _splitpath

typedef DIR* vm_findptr;

/*
 * findfirst, findnext, findclose direct emulation
 * for old ala Windows applications
 */
struct _finddata_t
{
  Ipp32u attrib;
  long long size;
  vm_char  name[260];
};

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

vm_findptr vm_string_findfirst(vm_char* filespec, vm_finddata_t* fileinfo);
Ipp32s vm_string_findnext(vm_findptr handle, vm_finddata_t* fileinfo);
Ipp32s vm_string_findclose(vm_findptr handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
#include <io.h>
#include <tchar.h>

#define VM_STRING(x) __T(x)
typedef TCHAR vm_char;
#define vm_string_printf    _tprintf
#define vm_string_fprintf   vm_file_fprintf
#define vm_string_sprintf   _stprintf
#define vm_string_snprintf  _sntprintf
#define vm_string_vprintf   _vtprintf
#define vm_string_vfprintf  vm_file_vfprintf
#define vm_string_vsprintf  _vstprintf

#define vm_string_strcat    _tcscat
#define vm_string_strncat   _tcsncat
#define vm_string_strcpy    _tcscpy
#define vm_string_strcspn   _tcscspn
#define vm_string_strspn    _tcsspn

#define vm_string_strlen    _tcslen
#define vm_string_strcmp    _tcscmp
#define vm_string_strncmp   _tcsnccmp
#define vm_string_stricmp   _tcsicmp
#define vm_string_strnicmp   _tcsncicmp
#define vm_string_strncpy   _tcsncpy
#define vm_string_strrchr   _tcsrchr

#define vm_string_atoi      _ttoi
#define vm_string_atof      _tstof
#define vm_string_atol      _ttol
#define vm_string_strstr    _tcsstr
#define vm_string_sscanf    _stscanf
#define vm_string_strchr    _tcschr
#define vm_string_strtok    _tcstok

#ifndef _WIN32_WCE
# define vm_findptr intptr_t
# define vm_finddata_t struct _tfinddata_t
# define vm_string_splitpath _tsplitpath
# define vm_string_findclose _findclose

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

vm_findptr vm_string_findfirst(vm_char* filespec, vm_finddata_t* fileinfo);
Ipp32s vm_string_findnext(vm_findptr handle, vm_finddata_t* fileinfo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* no findfirst etc for _WIN32_WCE */

#define vm_string_makepath  _tmakepath

#endif /* WINDOWS */

#define __VM_STRING(str) VM_STRING(str)

#endif /* __VM_STRINGS_H__ */
