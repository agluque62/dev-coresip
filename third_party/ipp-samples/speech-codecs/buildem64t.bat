@echo off
rem
rem                  INTEL CORPORATION PROPRIETARY INFORMATION
rem     This software is supplied under the terms of a license agreement or
rem     nondisclosure agreement with Intel Corporation and may not be copied
rem     or disclosed except in accordance with the terms of that agreement.
rem          Copyright(c) 2007-2009 Intel Corporation. All Rights Reserved.
rem

cls



REM
REM Usage:
REM  build{32|64|em64t}.bat [ cl7 | cl8 | icl91 | icl101 | icl110 | icl111 | ipc2009 ]
REM

::::::::::::::::::
setlocal
set ARG=%1

@CALL:GET_VARS

@%HEADER%
@%GET_ENVIRONMENT%

REM @echo setup directories
set TMP_FILE="%CD%.cmd"
echo set __LDN=%%~n0> %TMP_FILE%
call %TMP_FILE%
del %TMP_FILE%

REM @echo Delete old files
set BASE_PATH=%CD%
set GLOBAL_STATUS=-OK
set ROOT_DIR=.\_bin\%ARCH%_%COMP%
set LOG_DIR=.\_log\%ARCH%_%COMP%

if exist "%ROOT_DIR%" rd /S/Q "%ROOT_DIR%"
if exist "%LOG_DIR%"  rd /S/Q "%LOG_DIR%"

REM @echo Prepare log files
mkdir "%LOG_DIR%\core" "%LOG_DIR%\application" "%LOG_DIR%\io" "%LOG_DIR%\codec"
mkdir "%ROOT_DIR%"
set LOG_FILE="%LOG_DIR%\common.log"
set ERR_FILE="%LOG_DIR%\_compilation.failed"

call %TOOL%   %icl_opt%                                                                    > %LOG_FILE% 2>&1
call %IPPENV%                                                                   >> %LOG_FILE% 2>&1

REM @echo Setup log files
set CHCK_FILE="..\..\_passed_%ARCH%_%COMP%"
set ROOT_DIR=..\..\_bin\%ARCH%_%COMP%
set LOG_DIR=..\..\_log\%ARCH%_%COMP%

set all_comp_list=core\vm codec\speech application\usc_speech_codec application\usc_nr application\usc_tones application\usc_ec

@echo Start components compile

set NMAKE=nmake /nologo -f "%BASE_PATH%\Makefile"
if not exist ".\Makefile" (
  echo ERROR: Make file not found. Exit.
  goto end
)

for %%i in ( %all_comp_list% ) do (
  cd "%%i"
    setlocal ENABLEDELAYEDEXPANSION

    set OBJRES=
    set SFX=
    set OBJS=
    set BINNAME=!BINNAME!%%~ni
    set MODE=
    
    set SFX=!SFX!.lib
    
    for /F "tokens=1,2* delims=\\" %%a in ("%%i") do (
        set MODE=%%b
        if "%%a"=="application" (
           set SFX=.exe
        )
    )

    if exist src (
        for /F "usebackq" %%k in (`dir /a /b src\*.c*`) do set OBJS=!OBJS! ^$^(ObjDir^)\%%~nk.obj
    ) else (
        for /F "usebackq" %%l in (`dir /a /b *`) do (
           if exist %%l\src for /F "usebackq" %%k in (`dir /a /b %%l\src\*.c*`) do (
             set OBJS=!OBJS! ^$^(ObjDir^)\%%~nk.obj
           )
        )
    )
    set OBJS=!OBJS! ^$^(OBJRES^)
    set CC=%COMPILER%

    %NMAKE% >> "%LOG_DIR%\%%i.log" 2>&1

    if exist %CHCK_FILE% (
        echo *******  %%i     : PASSED
        echo %%i >> "%LOG_DIR%\_compilation.success"
    ) else (
        echo *******  %%i   : FAILED
        echo %%i >> "%LOG_DIR%\_compilation.failed"
    )

    endlocal

  cd ..\..
)

:end

IF EXIST "%ERR_FILE%" (
  SET GLOBAL_STATUS=-FAIL
  @call:err_a
)
@%FOOTER%

endlocal

GOTO:EOF
::::::::::::::::::::

:GET_VARS
set SCRIPT=%~n0
set ENV_FILE=env.bat
SET PLTF=%SCRIPT:build=%
SET ARCH=win%PLTF%
IF "%PLTF%" == "32"    SET ARCH_C=
IF "%PLTF%" == "64"    SET ARCH_C=64
IF "%PLTF%" == "em64t" SET ARCH_C=em64t
IF "%PLTF%" == "wcex86" SET ARCH_C=
IF "%PLTF%" == "wceixp" SET ARCH_C=ixp
SET x=%~d0%~p0
SET y=%x:\ipp-samples\=*%
FOR /F "tokens=1 delims=*" %%i IN ("%y%") DO SET SAMPLES_ROOT=%%i\ipp-samples\
SET HEADER=CALL "%SAMPLES_ROOT%\tools\env\tools" TYPE_HEADER
SET FOOTER=CALL "%SAMPLES_ROOT%\tools\env\tools" TYPE_FOOTER
SET GET_ENVIRONMENT=PUSHD "%SAMPLES_ROOT%\tools\env" ^& CALL %ENV_FILE% ^& POPD
EXIT /B

:err_a
@echo There were errors found
exit /B -1
