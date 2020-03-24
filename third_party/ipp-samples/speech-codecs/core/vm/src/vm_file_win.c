/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2008 Intel Corporation. All Rights Reserved.
//
*/

/*
 * VM 64-bits buffered file operations library
 *       Windows implementation
 */

/* codecws compilation fence */
#if !defined(LINUX32) && !defined(OSX)
# include <stdarg.h>
# include <io.h>
# include "ippdefs.h"
# include "vm_strings.h"
# include "vm_file.h"

#define STRBYTES(BFR) vm_string_strlen(BFR)*sizeof(vm_char)

#define DELETE_VM_FILE(A) \
  if (A != NULL) \
  { \
    if (A[0].tbuf != NULL) \
      free((void *)A[0].tbuf); \
 \
    free((void *)A); \
    A = NULL; \
  }

#ifdef THREAD_SAFETY
# define LOCK  vm_mutex_lock(fd[0].fd)
# define UNLOCK vm_mutex_unlock(fd[0].fd)
#else
# define LOCK
# define UNLOCK
#endif


#define VM_MAX_TEMP_LINE 8192

static vm_char temporary_path[MAX_PATH];

static BOOL tmpup = FALSE;
/*
 * low level temporary file support data
 * structures and functions
 */

typedef struct
{
  void*    next;
  void*    prev;
  vm_file* handle;
  vm_char* path;

} vm_temp_file_entry;

static
vm_temp_file_entry* temp_list_head;


static
void tempfile_add_entry(vm_temp_file_entry* r)
{
  if (temp_list_head != NULL)
  {
    r[0].next = temp_list_head;
    temp_list_head[0].prev = r;
    temp_list_head = r;
  }
  else
    temp_list_head = r;

  return;
} /* tempfile_add_entry() */


static
void tempfile_remove_entry(vm_temp_file_entry* r)
{
    vm_temp_file_entry* p;
    vm_temp_file_entry* q;
    BOOL sts;

    sts = TRUE;

    if ((p = temp_list_head) != NULL)
    {
        while( sts )
        {
            if(p == r)
            {
                if (p[0].prev == NULL)
                {
                    q = (vm_temp_file_entry *)p[0].next;
                    if (q != NULL)
                        q[0].prev = NULL;
                    temp_list_head = q;
                }
                else
                {
                    q = (vm_temp_file_entry *)p[0].prev;
                    q[0].next = p[0].next;
                    q = (vm_temp_file_entry *)p[0].next;
                    if (q != NULL)
                    {
                      q[0].prev = p[0].prev;
                      if (q[0].prev == NULL)
                        temp_list_head = q;
                    }
                }

                free(p[0].path);
                free(p);
                p = NULL;
            }
            else
                p = (vm_temp_file_entry *)p[0].next;

            sts = (p != NULL);
        }
    }

    return;
} /* tempfile_remove_entry() */


static
void tempfile_delete_by_handle(vm_file* fd)
{
    vm_temp_file_entry* p;

    if ((p = temp_list_head) != NULL)
    {
        while(p != NULL)
            if (p[0].handle == fd)
            {
                vm_file_remove(p[0].path);
                tempfile_remove_entry(p);
                return;
            }
            else
                p = (vm_temp_file_entry *)p[0].next;
    }

    return;
} /* tempfile_delete_by_handle() */


static
void tempfile_delete_all(void)
{
    vm_temp_file_entry* p;
    vm_temp_file_entry* q;

    if ((p = temp_list_head) != NULL)
    {
        while ( p != NULL)
        {
            q = p;
            p = (vm_temp_file_entry *)p[0].next;
            vm_file_close(q[0].handle); /* close and delete if temporary file is being closed */
        }
    }

    return;
} /* tempfile_delete_all() */


void vm_file_remove_all_temporary( void )
{
    tempfile_delete_all();
    return;
}


static
vm_file* tempfile_create_file(vm_char* fname)
{
    Ipp32u   nlen;
    vm_file* rtv;
    vm_temp_file_entry* t;

    rtv = NULL;

    nlen = (Ipp32u)STRBYTES(fname) + 1;

    t = (vm_temp_file_entry*)malloc(sizeof(vm_temp_file_entry));

    if (t != NULL)
    {
        t[0].handle = NULL;
        t[0].next = t[0].prev = NULL;
        t[0].path = (vm_char*)malloc(nlen*sizeof(vm_char));

        if(t[0].path != NULL)
        {
            vm_string_strcpy(t[0].path, fname);

            if((t[0].handle = vm_file_fopen(t[0].path, _T("w+"))) != NULL)
            {
                rtv = t[0].handle;
                t[0].handle[0].ftemp = VM_TEMPORARY_PREFIX;
                tempfile_add_entry(t);
            }
        } else
            free(t);
    }

    return rtv;
} /* tempfile_create_file() */


static
void tempfile_check(void)
{
  if (!tmpup)
  {
#ifndef _WIN32_WCE
  if (GetTempPath(MAX_PATH, temporary_path) == 0)
    vm_string_strcpy(temporary_path, _T("C:\\"));
#else
    vm_string_strcpy(temporary_path, _T("\\Windows"));
#endif
    atexit(tempfile_delete_all);
  }

  tmpup = TRUE;

  return;
} /* tempfile_check() */

/* fname == NULL - name will be generated, otherwise - fname will be used
 */
vm_file* vm_file_tmpfile(void)
{
    vm_file* rtv;
    vm_char* nbf;

      tempfile_check();

      rtv = NULL;

      nbf = (vm_char*)malloc(MAX_PATH*sizeof(vm_char));
      if (nbf != NULL) {
        if(vm_file_tmpnam(nbf) != NULL)
            rtv = tempfile_create_file(nbf);

        free(nbf);
      }
    return rtv;
} /* vm_file_tmpfile() */


vm_char* vm_file_tmpnam(vm_char* RESULT)
{
    vm_char* rtv;

    tempfile_check();

    rtv = NULL;

    if (RESULT == NULL)
        RESULT = (vm_char*)malloc(MAX_PATH*sizeof(vm_char));

    if (RESULT != NULL)
    {
        if (GetTempFileName(temporary_path, _T("vmt"), 0,  RESULT) != 0)
        {
            rtv = RESULT;
            DeleteFile(rtv);  /* we need only name, not a file */
        }
    }

    return rtv;
} /* vm_file_tmpnam() */


vm_char* vm_file_tmpnam_r(vm_char* RESULT)
{
    tempfile_check();

    if (RESULT != NULL)
        vm_file_tmpnam(RESULT);

    return RESULT;
} /* vm_file_tmpnam_r() */


vm_char* vm_file_tempnam(vm_char* DIR, vm_char* PREFIX)
{
    vm_char* nbf;

    tempfile_check();

    if ((nbf = (vm_char*)malloc(MAX_PATH*sizeof(vm_char))) != NULL) {
       if ((DIR == NULL) || (PREFIX == NULL))
           vm_file_tmpnam(nbf);
       else {
          if (GetTempFileName(DIR, PREFIX, 0,  nbf) != 0) {
              DeleteFile(nbf);  /* we need only name, not a file */
           } else {
            free(nbf);
            nbf = NULL;
        }
       }
      }
    return nbf;
} /* vm_file_tempnam() */


/*
 * file access functions
 */

vm_file* vm_file_fopen(const vm_char* fname, const vm_char* mode)
{
  Ipp32u   i;
  DWORD    mds[4];
  vm_char  general;
  vm_char  islog;
  vm_file* rtv;

  general = islog = 0;

  rtv = (vm_file*)malloc(sizeof(vm_file));
  if (rtv != NULL)
  {
    rtv[0].fd          = NULL;
    rtv[0].fsize       = 0;
    rtv[0].fattributes = 0;
    rtv[0].ftemp       = 0;
    rtv[0].fbin        = 0;

    rtv[0].lnpos       = 0;
    rtv[0].lnptr       = NULL;
    rtv[0].lnchar      = '\0';
    vm_file_getinfo(fname, &rtv[0].fsize, &rtv[0].fattributes);
    memset((void *)&rtv[0].lck, 0, sizeof(vm_mutex));

    vm_mutex_init(&rtv[0].lck);

    rtv[0].tbuf = (vm_char*)malloc(VM_MAX_TEMP_LINE*sizeof(vm_char));
    if ((rtv[0].tbuf != NULL) && vm_mutex_is_valid(&rtv[0].lck)) {
      rtv[0].tbuf[0] = '\0';
      /* prepare dwDesiredAccess */
      mds[0] = mds[1] = mds[2] = mds[3] = 0;

      for(i = 0; mode[i] != '\0'; ++i)
      {
        switch (mode[i])
        {
          case 'w':
            mds[0] |= GENERIC_WRITE;
            mds[2]  = CREATE_ALWAYS;
            mds[1]  = FILE_SHARE_READ;
            general = 'w';
            break;

          case 'r':
            mds[0] |= GENERIC_READ;
            mds[2]  = OPEN_EXISTING;
            mds[1]  = FILE_SHARE_READ;
            general = 'r';
            break;

          case 'a':
            mds[0] |= (GENERIC_WRITE | GENERIC_READ);
            mds[2]  = OPEN_ALWAYS;
            mds[1]  = 0;
            general = 'a';
            break;

          case '+':
            mds[0] |= (GENERIC_WRITE | GENERIC_READ);
            mds[1]  = FILE_SHARE_READ;
            break;

          case 'l':
            islog = 1; /* non-buffered file - for log files */

          case 'b':
            rtv[0].fbin = 1; /* file for binary access - no eol translation required */
        }
      }

      mds[3] = FILE_ATTRIBUTE_NORMAL | (islog == 0) ? 0 : FILE_FLAG_NO_BUFFERING;

      rtv[0].fd = CreateFile((LPCTSTR)fname, mds[0], mds[1], NULL, mds[2], mds[3], NULL);
      if (rtv[0].fd == INVALID_HANDLE_VALUE)
      {
        DELETE_VM_FILE(rtv);
      }
      else
      {
        /* check file open mode and move file pointer to the end of file if need it */
        if (general == 'a')
          vm_file_fseek(rtv, 0, VM_FILE_SEEK_END);
      }
    }
  }

  return rtv; /* handle created - have to be destroyed by close function */
} /* vm_file_fopen() */

Ipp32s vm_file_fflush(vm_file *fd) {
  Ipp32s rtv = -1;
  if (fd == vm_stdout)
      rtv = fflush(stdout);
  else if (fd == vm_stderr)
      rtv = fflush(stderr);
  else
      rtv = (FlushFileBuffers(fd[0].fd)) ? 0 : 1;
  return rtv;
  }

Ipp64u vm_file_fseek(vm_file* fd, Ipp64s position, VM_FILE_SEEK_MODE mode)
{
  Ipp64u rtv   = 1;
  DWORD  posmd = 0;

  union
  {
    Ipp32s lpt[2];
    Ipp64s hpt;
  } pwt;

  pwt.hpt = position;

  switch (mode)
  {
    case VM_FILE_SEEK_END: posmd = FILE_END;     break;
    case VM_FILE_SEEK_SET: posmd = FILE_BEGIN;   break;
    case VM_FILE_SEEK_CUR:
    default:               posmd = FILE_CURRENT; break;
  }

  if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
  {
    LOCK;
    pwt.lpt[0] = SetFilePointer(fd[0].fd, pwt.lpt[0], (LONG *)&pwt.lpt[1], posmd);
    if ((pwt.lpt[0] != -1) || (GetLastError() == NO_ERROR))
      rtv = 0;
    UNLOCK;
  }

  return rtv;
} /* vm_file_fseek() */


Ipp64u vm_file_ftell(vm_file* fd)
{
  Ipp64u rtv = 0;

  union
  {
    Ipp32s lpt[2];
    Ipp64s hpt;
  } pwt;

  pwt.hpt = 0; // query current file position

  if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
  {
    LOCK;
    pwt.lpt[0] = SetFilePointer(fd[0].fd, pwt.lpt[0], (LONG *)&pwt.lpt[1], FILE_CURRENT);
    if ((pwt.lpt[0] != -1) || (GetLastError() == NO_ERROR))
      rtv = (Ipp64u)pwt.hpt;
    UNLOCK;
  }

  return rtv;
} /* vm_file_ftell() */


Ipp32s vm_file_remove(vm_char* path)
{
  return DeleteFile(path);
}


Ipp32s vm_file_fclose(vm_file* fd)
{
  Ipp32s rtv = 0;

  if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
  {
    LOCK;
    CloseHandle(fd[0].fd);
    UNLOCK;

    /* delete file if temporary file is being closed (according to IEC/ISO specification) */
    if (fd[0].ftemp == VM_TEMPORARY_PREFIX)
        tempfile_delete_by_handle(fd);
    /* return memory, allocated in CreateFile */
    DELETE_VM_FILE(fd);
    rtv = 1;
  }

  return rtv;
} /* vm_file_fclose() */


Ipp32s vm_file_feof(vm_file* fd)
{
  Ipp32s rtv = 0;

  if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
  {
    Ipp64u pos = vm_file_ftell(fd);
    if (pos >= fd[0].fsize)
      rtv = 1;
  }

  return rtv;
} /* vm_file_feof() */


Ipp32s vm_file_getinfo(const vm_char* filename, Ipp64u* file_size, Ipp32u* file_attr)
{
  Ipp32s rtv = 0;
  Ipp32s needsize = (file_size != NULL);
  Ipp32s needattr = (file_attr != NULL);
  WIN32_FILE_ATTRIBUTE_DATA ffi;

  if (filename && (needsize || needattr))
  {
    if (GetFileAttributesEx(filename, GetFileExInfoStandard, &ffi) != 0)
    {
      if (needattr)
      {
        file_attr[0] += (ffi.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? VM_FILE_ATTR_HIDDEN : 0;
        file_attr[0] += (ffi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? VM_FILE_ATTR_DIRECTORY : 0;
        file_attr[0] += (ffi.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) ? VM_FILE_ATTR_FILE : 0;
      }

      if (needsize)
      {
        file_size[0] = ffi.nFileSizeHigh; // two steps to avoid 32 to 32 shift problem
        file_size[0] = ffi.nFileSizeLow + ( file_size[0] << 32);
      }

      rtv = 1;
      }
    }

  return rtv;
} /* vm_file_getinfo() */


/* binary file IO */

Ipp32s vm_file_fread(void* buf, Ipp32u itemsz, Ipp32s nitems, vm_file* fd)
{
  DWORD  nmbread = 0;
  Ipp32s rtv     = 0;

  if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
  {
    LOCK;
    rtv = (ReadFile(fd[0].fd, buf, nitems*itemsz, &nmbread, NULL)) ? (nmbread/itemsz) : 0;
    UNLOCK;
  }

  return rtv;
} /* vm_file_fread() */


Ipp32s vm_file_fwrite(void* buf, Ipp32u itemsz, Ipp32s nitems, vm_file* fd)
{
  DWORD  nmbread;
  Ipp32s rtv = 0;

  if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
  {
    LOCK;
    rtv = (WriteFile(fd[0].fd, buf, nitems*itemsz, &nmbread, NULL)) ? (nmbread/itemsz) : 0;
    UNLOCK;
  }

  return rtv;
} /* vm_file_fwrite() */


/*
 * character (string) file IO
 */

/*
 * read characters from input file until newline found, null character found,
 * nchars rad or EOF reached
 */

vm_char* vm_file_fgets(vm_char* str, int nchar, vm_file* fd)
{
  /* read up to VM_MAX_TEMP_LINE characters from input file, try
   * to satisfy fgets conditions
   */
  Ipp64s fpos;
  Ipp32s rdchar, i, j = 0; // j - current position in the output string
  vm_char* rtv = NULL;

  if (fd == vm_stdin)
#ifdef _UNICODE
    return fgetws(str, nchar, stdin);
#else
    return fgets(str, nchar, stdin);
#endif
  else
  {
    str[0] = str[nchar-1] = 0;
    --nchar;

    if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
    {
      while ((rdchar = vm_file_fread(fd[0].tbuf, 1, VM_MAX_TEMP_LINE, fd)/sizeof(vm_char)) != 0)
      {
        for(i = 0; i < rdchar; ++i)
        {
          str[j] = fd[0].tbuf[i];
          if((str[j]==0) || (str[j]=='\n') || (j >= nchar))
            break;
          ++j;
        }

        if (i < rdchar)
        {
          /* one of EOS conditions found */
          if ((str[j] == '\n') && (j < nchar)) /* add null character if possible */
            str[++j] = 0;
#if 0
          if (str[j-2] == '\r')
          {
            /* remove CR from end of line */
            str[j-2] = '\n';
            str[j-1] = '\0';
          }
#endif
          /* return file pointer to the first non-string character */
          ++i; // skip end of line character
          i *= sizeof(vm_char);
          fpos = i - (Ipp64s)(rdchar*sizeof(vm_char)); // - -- because file pointer will move back
          if (fpos != 0)
            vm_file_fseek(fd, fpos, VM_FILE_SEEK_CUR);

          rtv = str;
          break; // leave while loop
        }
      }

      if((rtv == NULL) && (j != 0) && vm_file_feof(fd))
      {
        rtv = str; // end of file during read - input string is valid
        if (j < nchar)
          str[++j] = 0;
      }
    }
  }

  return rtv;
} /* vm_file_fgets() */


#ifdef _UNICODE
# define FPUTS fputws
# define FPUTSPROBLEM WEOF
# define FPUTSEOLBYTES 4
#else
# define FPUTS fputs
# define FPUTSPROBLEM EOF
# define FPUTSEOLBYTES 2
#endif
Ipp32s crcounts( vm_char *s) {
  Ipp32s rtv = 0;
  while(*s)
    if (*s++ == '\n')
      ++rtv;
  return rtv;
  }

static void extcpy(vm_char *to, vm_char *from) {
  while(*from) {
    if(*from == '\n') {
      *to++ = '\r';
      *to++ = '\n';
      } else
        *to++ = *from;
      ++from;
    }
  *to = *from;
  }

Ipp32s vm_file_fputs(vm_char* str, vm_file* fd)
{
  Ipp32s rtv = FPUTSPROBLEM;
  vm_char *ostr = NULL;
  if ( fd == vm_stdout )
    rtv = FPUTS(str, stdout);
  else
    if (fd == vm_stderr)
      rtv = FPUTS(str, stderr);
    else {
      if (fd[0].fbin) 
        rtv = (vm_file_fwrite((void*)str, 1, (Ipp32u)STRBYTES(str), fd)==(Ipp32s)(STRBYTES(str))) ? 1 : FPUTSPROBLEM;
      else
        ostr = malloc(STRBYTES(str)+(crcounts(str)+1)*FPUTSEOLBYTES);
        if (ostr != NULL) {
          extcpy(ostr, str);
          rtv = (vm_file_fwrite((void*)ostr, 1, (Ipp32u)STRBYTES(ostr), fd)==(Ipp32s)(STRBYTES(ostr))) ? 1 : FPUTSPROBLEM;
          free(ostr);
          }
    }
    return rtv;
  }


/* parse line, return total number of format specifiers */
Ipp32s fmtline_spcnmb( vm_char* s )
{
  Ipp32s rtv = 0;

  while ( *s )
    if ((*s++ == '%') && (*s != '%'))
      ++rtv;

  return rtv;
}
/*
 pseudo-reentrant token returning function
 default delimiters are ",", "\n"
*/
static vm_char *gettoken(vm_char *ln, Ipp32s *skippedbytes, vm_file *fd) {
  vm_char *rtv = NULL;
  Ipp32s frompos = 0;
  skippedbytes[0] = 0;
  if (ln != NULL) {
    fd[0].lnptr = ln;
    fd[0].lnpos = 0;
    fd[0].lnchar = ln[fd[0].lnpos];
    }
  fd[0].lnptr[fd[0].lnpos] = fd[0].lnchar;
  frompos = fd[0].lnpos;
  while( (fd[0].lnptr[fd[0].lnpos] == ' ') ||
    (fd[0].lnptr[fd[0].lnpos] == '\t') ) {
    if (fd[0].lnptr[fd[0].lnpos] == '\0')
      break;
    ++skippedbytes[0];
    ++frompos;
    ++fd[0].lnpos;
    }
  rtv = fd[0].lnptr + frompos;
  while( (fd[0].lnptr[fd[0].lnpos] != ' ') &&
         (fd[0].lnptr[fd[0].lnpos] != 0) &&
         (fd[0].lnptr[fd[0].lnpos] != ',') &&
         (fd[0].lnptr[fd[0].lnpos] != '\n') &&
         (fd[0].lnptr[fd[0].lnpos] != '\r') ) {
           ++skippedbytes[0];
           ++fd[0].lnpos;
    }
  fd[0].lnchar = fd[0].lnptr[fd[0].lnpos];
  fd[0].lnptr[fd[0].lnpos] = '\0';
  return (fd[0].lnptr[frompos] != '\0') ? rtv : NULL;
  }

static vm_char *getfiletoken(vm_file *fd) {
  vm_char *rtv = NULL;
  vm_char *s = NULL;
  vm_char* ws = NULL;
  vm_char* qs = NULL;
  Ipp32s items = 0;
  Ipp32s skipbytes = 0;
  Ipp32s gobackcounter = 0;
  if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE)) {
    s = (vm_char*)malloc(VM_MAX_TEMP_LINE*sizeof(vm_char));
    if (s != NULL) {
      do {
        ws = vm_file_fgets(s, VM_MAX_TEMP_LINE, fd);
        s[VM_MAX_TEMP_LINE-1] = '\0'; /* protect from potential line length problems */
        vm_string_strcpy(fd[0].tbuf, s);
        if (ws != NULL) {
          gobackcounter = (Ipp32s)vm_string_strlen(fd[0].tbuf);
          qs = gettoken(fd[0].tbuf, &skipbytes, fd);
          }
        } while( (qs == NULL) && (vm_file_feof(fd) == 0));
      if (qs != NULL) {
        rtv = qs;
        gobackcounter -= skipbytes;
        }
      else
        gobackcounter = 0;
      }
    }
  free(s);
  /* return the rest of string to file - move file pointer back */
  if (gobackcounter > 0)
    vm_file_fseek(fd, 0 - (Ipp64s)(gobackcounter*sizeof(vm_char)), VM_FILE_SEEK_CUR);
  return rtv;
  }

Ipp32s vm_file_fscanf(vm_file *fd, vm_char *format, ...) {
  BOOL     eol = FALSE;
  Ipp32u   i = 0;
  Ipp32s   rtv = 0;
  Ipp32s   items = 0;
  vm_char* fpt = NULL;
  vm_char* bpt = NULL;
  vm_char* ws = NULL;
  va_list  args;
  vm_char  tmp = '\0';

  if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE)) {
    items = fmtline_spcnmb( format );
    ws = getfiletoken(fd);
    /* try to convert all parameters on the step by step basis */
    if ( (ws != NULL) && ((bpt = fpt = (vm_char *)malloc(STRBYTES(format)+16)) != NULL)) {
      vm_string_strcpy(fpt,format);
      va_start( args, format );
      while(items--) {
        while((*fpt != '%') && (*fpt))
          ++fpt;
        tmp = '\0';
        if( items > 0) {
          for(i = 1; (fpt[i] != '\0' && fpt[i] != '\n' &&
                    fpt[i] != ' '  && fpt[i] != '%' &&
                    fpt[i] != ','); ++i)
                    ;
          if ((fpt[i] != '\0') && (fpt[i]!='\n')) {
            tmp    = fpt[i];
            fpt[i] = '\0';
            }
          }
          if (*fpt)
#ifdef _UNICODE
            rtv += swscanf(ws, fpt, va_arg(args, void*));
#else
            rtv += _stscanf(ws, fpt, va_arg(args, void*));
#endif
            else
              items = 0;
            if (items > 0) {
              ws = getfiletoken(fd);
              fpt[i] = tmp;
              ++fpt;
              if (ws == NULL)
                items = 0;
              }
        }
        va_end( args );
        eol = TRUE;
        free(bpt);
      } while (!eol);
    }
  return rtv;
} /* vm_file_fscanf() */


/*
 * Error may occur if output string becomes longer than VM_MAX_TEMP_LINE
 */

Ipp32s vm_file_fprintf(vm_file *fd, vm_char *format, ...)
{
  Ipp32s  rtv = 0;
  va_list args;

  va_start( args, format );

  if (fd == vm_stdout)
    rtv = _vftprintf(stdout, format, args);
  else
  {
    if (fd == vm_stderr)
      rtv = _vftprintf(stderr, format, args);
    else
    {
      if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
      {
        vm_string_vsprintf( fd[0].tbuf, format, args );
        if (vm_file_fputs(fd[0].tbuf, fd) > 0)
          rtv = (Ipp32s)STRBYTES(fd[0].tbuf)+(crcounts(fd[0].tbuf)+1)*FPUTSEOLBYTES;
        va_end( args );
      }
    }
  }
  return rtv;
} /* vm_file_fprintf() */


Ipp32s vm_file_vfprintf(vm_file* fd, vm_char* format,  va_list argptr)
{
  Ipp32s rtv = 0;

  if (fd == vm_stdout)
    _vftprintf(stdout, format, argptr);
  else
  {
    if (fd == vm_stderr)
      _vftprintf(stderr, format, argptr);
    else
    {
      if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
      {
        vm_string_vsprintf( fd[0].tbuf, format, argptr );
        if (vm_file_fputs(fd[0].tbuf, fd) > 0)
          rtv = (Ipp32s)STRBYTES(fd[0].tbuf)+(crcounts(fd[0].tbuf)+1)*FPUTSEOLBYTES;
      }
    }
  }
  return rtv;
} /* vm_file_vfprintf() */


/* Directory manipulations */

Ipp32s vm_dir_remove(vm_char* path)
{
  Ipp32s rtv = 1;

  if (!DeleteFile(path))
    rtv = RemoveDirectory(path);

  return rtv;
}


Ipp32s vm_dir_mkdir(vm_char* path)
{
   return CreateDirectory(path, NULL);
}


Ipp32s vm_dir_open(vm_dir** dd, vm_char* path)
{
  Ipp32s rtv = 0;
  WIN32_FIND_DATA fdata;

  vm_dir* td = (vm_dir*)malloc(sizeof(vm_dir));

  if (td != NULL)
  {
    td->handle = FindFirstFile(path, &fdata/*&td->ent*/);
    *dd = td;

    if (td->handle != INVALID_HANDLE_VALUE)
      rtv = 1;
  }

  return rtv;
} /* vm_dir_open() */


Ipp32s vm_dir_read(vm_dir* dd, vm_char* filename,int nchars)
{
  Ipp32s rtv = 0;
  WIN32_FIND_DATA fdata;

  if (dd->handle != INVALID_HANDLE_VALUE)
  {
    if (FindNextFile(dd->handle,&fdata/*&dd->ent*/))
    {
      rtv = 1;
      vm_string_strncpy(filename,fdata.cFileName/*dd->ent.cFileName*/,min(nchars,MAX_PATH));
    }
    else
      FindClose(dd->handle);

    dd->handle = INVALID_HANDLE_VALUE;
   }

  return rtv;
} /* vm_dir_read() */


void vm_dir_close(vm_dir *dd)
{
  if (dd->handle!=INVALID_HANDLE_VALUE)
  {
      FindClose(dd->handle);
      free(dd);
  }

  return;
} /* vm_dir_close() */


Ipp64u vm_dir_get_free_disk_space( void )
{
  ULARGE_INTEGER freebytes[3];

  GetDiskFreeSpaceEx(VM_STRING("."), &freebytes[0], &freebytes[1], &freebytes[2]);

  return freebytes[0].QuadPart; /* return total number of available free bytes on disk */
}

#ifndef _WIN32_WCE
/*
 * fndfirst, findnext with correct (for VM) attributes
 */

static
unsigned int reverse_attribute(unsigned int attr)
{
   unsigned int rtv = 0;

   if ((attr & _A_SUBDIR) == _A_SUBDIR)
     rtv |= VM_FILE_ATTR_DIRECTORY;
   else
     rtv |= VM_FILE_ATTR_FILE;

   if ((attr & _A_HIDDEN) == _A_HIDDEN)
     rtv |= VM_FILE_ATTR_HIDDEN;

   return rtv;
} /* reverse_attribute() */


#define ADDNUM 16

vm_findptr vm_string_findfirst(vm_char* filespec, vm_finddata_t* fileinfo)
{
  vm_findptr pv = 0;
  size_t i;
  vm_char* tmppath;

  /* check file path and add wildcard specification if missed */
  i = vm_string_strlen(filespec);

  tmppath = (vm_char*)malloc((i+ADDNUM)*sizeof(vm_char));
  if (tmppath != NULL)
  {
    vm_string_strcpy(tmppath, filespec);

    if (i > 1)
      --i;

    if (tmppath[i] != '*')
    {
      if (tmppath[i] != '\\')
        vm_string_strcat(tmppath, VM_STRING("\\"));

      vm_string_strcat(tmppath, VM_STRING("*"));
    }

    if ((pv = _tfindfirst(tmppath, fileinfo)) > 0)
      fileinfo[0].attrib = reverse_attribute(fileinfo[0].attrib);
    free(tmppath);
  }

  return pv;
} /* vm_string_findfirst() */


Ipp32s vm_string_findnext(vm_findptr handle, vm_finddata_t *fileinfo)
{
  Ipp32s rtv = 0;

  if ((rtv = _tfindnext(handle, fileinfo)) == 0)
    fileinfo[0].attrib = reverse_attribute(fileinfo[0].attrib);

  return rtv;
} /* vm_string_findnext() */
#endif

#endif // Windows is not Unix
