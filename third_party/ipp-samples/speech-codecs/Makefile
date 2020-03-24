##
##                  INTEL CORPORATION PROPRIETARY INFORMATION
##     This software is supplied under the terms of a license agreement or
##     nondisclosure agreement with Intel Corporation and may not be copied
##     or disclosed except in accordance with the terms of that agreement.
##          Copyright(c) 2007-2008 Intel Corporation. All Rights Reserved.
##

.DEFAULT: all

OBJSFX = .obj
SrcDir = .\src

!IF "$(ROOT_BIN_DIR)" == ""
ROOT_BIN_DIR = ..\..\_bin\$(ARCH)_$(COMP)
!ENDIF

## Directory name for obj-files
ObjDir = $(ROOT_BIN_DIR)\obj\$(BINNAME)

## Directory name for exe-files
!IF "$(SFX)" == ".lib"
BinDirAdd = \lib
!ENDIF
BinDir = $(ROOT_BIN_DIR)$(BinDirAdd)

LINK32 = link.exe
RC     = rc.exe

## defined ARCH depended values

!IF "$(ARCH)" == "win32"
M_FLAG=/MD
LIB_ARCH  =
LINK_ARCH = I386
!ELSEIF "$(ARCH)" == "win64"
M_FLAG=/MD
LIB_ARCH  = 64
LINK_ARCH = IA64
!ELSEIF "$(ARCH)" == "winem64t"
M_FLAG=/MD
LIB_ARCH  = em64t
LINK_ARCH = AMD64
!ENDIF

RTA_LIB = /LIBPATH:..\..\lib\$(ARCH) ipp_rta.lib

##############################################################################

!IF "$(SFX)" == ".exe" || "$(SFX)" == ".dll"
ADD_LIB_PATH = /LIBPATH:$(ICLLIBPATH) /LIBPATH:"$(ROOT_BIN_DIR)\lib" /LIBPATH:"$(IPPROOT)\lib"

SYBSYSTEM_MODE = /subsystem:console

ADD_LIB = speech.lib \
          vm.lib

!IF "$(MODE)" == "umc_speech_rtp_codec"
ADD_LIB_WIN32 = umc.lib vm.lib audio_renders.lib media_buffers.lib jitter_buffers.lib speech_rtp.lib
!ENDIF

!IF "$(LINKAGE)" == ""
LINKAGE = dynamic
!ENDIF

IPPLIB = \
!IF "$(LINKAGE)" == "dynamic"
  ippcore$(LIB_ARCH).lib ippcore$(LIB_ARCH)l.lib\
  ipps$(LIB_ARCH).lib    \
  ippsc$(LIB_ARCH).lib   \
  ippsr$(LIB_ARCH).lib
!ELSE
  ippcore$(LIB_ARCH)l.lib \
!IF "$(ARCH)" == "win64"
  ippsi7l.lib  \
  ippsci7l.lib \
  ippsri7l.lib
!ELSE
  ippsemerged$(LIB_ARCH).lib ippsmerged$(LIB_ARCH).lib   \
  ippscemerged$(LIB_ARCH).lib ippscmerged$(LIB_ARCH).lib \
  ippsrmerged$(LIB_ARCH).lib ippsremerged$(LIB_ARCH).lib
!ENDIF
!ENDIF

SYSLIB = kernel32.lib user32.lib gdi32.lib comdlg32.lib \
	 advapi32.lib shell32.lib comctl32.lib winmm.lib pdh.lib


!ENDIF

CFLAGS = /W4 /O2 $(M_FLAG) /EHsc /GS /c

CINCLUDE = -I "$(IPPROOT)\include" \
         -I include \
         -I "..\..\core\vm\include" \
         -I "..\..\core\vm\include\sys" \
         -I "..\..\core\usc\include" \
         -I "..\..\io\audio_renders\include" \
         -I "..\..\io\media_buffers\include" \
         -I "..\..\io\jitter_buffers\include" \
         -I "..\..\codec\speech_rtp\include" \
         -I "..\..\core\umc\include" \
         -I "..\..\core\vm_plus\include" \
         -I "..\..\codec\speech\alc\include" \
         -I "..\..\codec\speech\amrwb\include" \
         -I "..\..\codec\speech\amrwbe\include" \
         -I "..\..\codec\speech\common\include" \
         -I "..\..\codec\speech\ec\include" \
         -I "..\..\codec\speech\g711\include" \
         -I "..\..\codec\speech\g722\include" \
         -I "..\..\codec\speech\g722.1\include" \
         -I "..\..\codec\speech\g723.1\include" \
         -I "..\..\codec\speech\g726\include" \
         -I "..\..\codec\speech\g728\include" \
         -I "..\..\codec\speech\g729fp\include" \
         -I "..\..\codec\speech\g729i\include" \
         -I "..\..\codec\speech\g729.1\include" \
         -I "..\..\codec\speech\gsmamr\include" \
         -I "..\..\codec\speech\gsmfr\include" \
         -I "..\..\codec\speech\nr\include" \
         -I "..\..\codec\speech\td\include" \
         -I "..\..\codec\speech\vad\include" \
         -I "..\..\codec\speech\vad_sr\include" \
         -I "..\..\codec\speech\msrtafp\include" \
         -I "..\..\codec\speech\msrta_msft\include"

!IF "$(ARCH)"=="win64"
!IF "$(COMP)"=="icl91"
CFLAGS = /W4 /O1 $(M_FLAG) /EHsc /GS /c	
!ENDIF
!ENDIF

!IF "$(CC)" == "icl.exe" || "$(CC)" == "ecl.exe"
CFLAGS = $(CFLAGS) $(ICL_OMPLIB_OPT) /Qwd9,171,188,593,810,981,1125,1418
!ENDIF

CDEFINES = /D "STRICT" /D "NDEBUG" $(CADD_DEFINE)

!IF "$(ARCH)" == "win32"
CDEFINES = $(CDEFINES) /D _WIN32 /D _WIN32_WINNT=0x0500 /D WIN32 /D CLIPPING_DENORMAL_MODE
SYSLIB = $(SYSLIB) $(ADD_SYSLIB_WIN32)
ADD_LIB = $(ADD_LIB) $(ADD_LIB_WIN32)
DLL_LIST_WIN = $(DLL_LIST) $(DLL_LIST_WIN32)
!ELSEIF "$(ARCH)" == "winem64t"
CDEFINES = $(CDEFINES) /D WIN64 /D _WIN64 /D _AMD64_ /D CLIPPING_DENORMAL_MODE
SYSLIB = $(SYSLIB) $(ADD_SYSLIB_WINEM64T)
ADD_LIB = $(ADD_LIB) $(ADD_LIB_WINEM64T)
DLL_LIST_WIN = $(DLL_LIST) $(DLL_LIST_WINEM64T)
!ELSEIF "$(ARCH)" == "win64"
CDEFINES = $(CDEFINES) /D WIN64 /D _WIN64 /D CLIPPING_DENORMAL_MODE
SYSLIB = $(SYSLIB) $(ADD_SYSLIB_WIN64)
ADD_LIB = $(ADD_LIB) $(ADD_LIB_WIN64)
DLL_LIST_WIN = $(DLL_LIST) $(DLL_LIST_WIN64)
!ENDIF

#!IF "$(MODE)" == "usc_speech_codec"
CDEFINES = $(CDEFINES) /D _USC_ALL

################################################################

ADD_LINK_OPT = 

!IF "$(COMP)" == "cl8"
!IF "$(SFX)" == ".exe"
ADD_LINK_OPT = $(ADD_LINK_OPT) /MANIFEST
!ENDIF
!ENDIF

LINK_CMD = $(LINK32) $(LINK_MOD) $(SYBSYSTEM_MODE) $(ADD_LINK_OPT) /nodefaultlib:"libcmt.lib" \
            /out:$@ $? $(ADD_LIB_PATH) $(SYSLIB) \
!IF "$(CC)" == "icl.exe" && ( "$(SFX)" == ".exe"  || "$(SFX)" == ".dll" )
           $(ICL_OMPLIB_LINK) /nodefaultlib:"libmmt.lib" /nodefaultlib:"libc.lib"\
!ENDIF
!IF ( "$(COMP)" == "cl8" && "$(ARCH)" == "win64" ) || ( "$(COMP)" == "cl7" && ( "$(ARCH)" == "win64" || "$(ARCH)" == "winem64t" ) )
!IFNDEF VISTASDK
  bufferoverflowu.lib \
!ENDIF
!ENDIF
           $(ADD_LIB) $(RTA_LIB) $(IPPLIB)

!IF "$(SFX)" == ".lib"
LINK32 = lib.exe
!ENDIF

all : prepare build postbuild

build : $(BinDir)\$(BINNAME)$(SFX)

!IF "$(MODE)" == "speech"
{.\alc\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\amrwb\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\amrwbe\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\common\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\ec\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\g711\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\g722\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\g722.1\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\g723.1\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\g726\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\g728\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\g729fp\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\g729i\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\gsmamr\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\gsmfr\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\nr\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\td\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\vad\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\vad_sr\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\msrtafp\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\msrta_msft\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
{.\g729.1\src}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
!ELSE
{$(SrcDir)}.cpp{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<

{$(SrcDir)}.c{$(ObjDir)}$(OBJSFX)::
	$(CC) $(CFLAGS) $(CINCLUDE) $(CDEFINES) /Fo$(ObjDir)\ /c $<
!ENDIF
$(BinDir)\$(BINNAME)$(SFX) : $(OBJS)
	$(LINK_CMD)

postbuild:
	@echo . > $(CHCK_FILE)

prepare:
        @echo "$(SFX)"
	@if exist $(CHCK_FILE) del /q $(CHCK_FILE)
	@if not exist $(ObjDir)   mkdir $(ObjDir)
	@if not exist $(BinDir)   mkdir $(BinDir)

clean:
	@echo Cleaning ...
	@if exist $(ObjDir) del /s /q $(OBJS)
	@if exist $(ObjDir) rd /s /q $(ObjDir)
	@del /s /q $(DLL_LIST_WIN)
	@if exist $(BinDir)\$(BINNAME)$(SFX) del /s /q $(BinDir)\$(BINNAME)$(SFX)
	@echo Cleaning complete

