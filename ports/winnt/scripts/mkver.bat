@echo off
GOTO PROG

see notes/remarks directly below this header:
######################################################################
#
# Revision: mkver.bat
# Author:   Frederick Czajka
# Date:     02/10/2000
# Purpose:  Provide a NT Shell script to replace the perl script 
#           that replaced the UNIX mkver shell script.
#           
# 
#
# Notes:  I had two goals with this script one to only use native
#         NT Shell commands and two was too emulate the PERL style
#         output. This required some work for the DATE format as 
#         you will see and TIME was really tricky to get a format 
#         matching PERLs!
#
#
# Changes:
# 02/23/2011	David J Taylor	- Use reg instead of regedit so "run as
#				  administrator" is not required.
# 12/21/2009	Dave Hart
#				- packageinfo.sh uses prerelease= now not
#				  releasecandidate=
# 08/28/2009	Dave Hart	
#				- support for building using per-compiler subdirs of winnt
# 08/08/2006	Heiko Gerstung
#				- bugfixed point / rcpoint errors leading to a wrong
#				  version string 
#				- added a few cases for uppercase strings
# 03/09/2005	Heiko Gerstung
#				- added UTC offset to version time information
#				- bugfixed several issues preventing this script to be used on NT4 
#				- removed an obsolete warning
#
# 03/08/2005	Danny Mayer
#				- bugfixed NOBK label position
#
# 03/08/2005	Heiko Gerstung
#				- bugfixed BK detection and support for multiple ChangeSets 
#				
# 02/24/2005	Heiko Gerstung
#				- check if BK is installed and do not try to call it if not
#
#
# 02/03/2005	Heiko Gerstung
#				- now getting NTP version from version.m4 (not configure)
#				- added BK ChangeSet revision and Openssl-Indicator (-o) 
#				  to the version number
#				- major rework of the time and date recognition routines
#				  in order to reflect international settings and OS-
#				  dependand formats
#
######################################################################

Notes/Howtos:

If you spot an error stating that bk.exe could not be found or executed
although it is installed on your computer, you should try to add the path 
to your BK binary in your IDE configuration (for VisualStudio see 
Tools/Options/Directories/Executables).

Alternatively you can create a file called 'version' in the root path of 
your ntp source tree which holds a string that is added to the version number.


:PROG
IF {%1} == {} GOTO USAGE
IF {%1} == {-H} GOTO USAGE
IF {%2} == {} GOTO USAGE
IF {%1} == {-P} GOTO BEGIN



REM *****************************************************************************************************************
REM For any other bizarre permutation...
REM *****************************************************************************************************************
GOTO USAGE

:BEGIN

SET GENERATED_PROGRAM=%2

REM *****************************************************************************************************************
REM Reimplemented from orginal Unix Shell script
REM *****************************************************************************************************************
	IF NOT EXIST .version ECHO 0 > .version
	FOR /F %%i IN (.version) do @SET RUN=%%i
	SET /A RUN=%RUN%+1
	ECHO %RUN% > .version

REM *****************************************************************************************************************
REM Resetting variables
REM *****************************************************************************************************************
	SET VER=
	SET CSET=
	SET SSL=
	SET MYDATE=
	SET MYTIME=
	SET DAY=99
	SET NMM=99
	SET YEAR=0
	SET HOUR=
	SET MIN=
	SET MMIN=
	SET SEC=
	SET SUBSEC=
	SET DATEDELIM=
	SET TIMEDELIM=
	SET DATEFORMAT=
	SET TIMEFORMAT=
	SET UTC=
	SET ACTIVEBIAS=

REM *****************************************************************************************************************
REM Check if DATE and TIME environment variables are available
REM *****************************************************************************************************************

	SET MYDATE=%DATE%
	SET MYTIME=%TIME%

	REM ** Not available (huh? Are you older than NT4SP6A, grandpa?)
	IF "%MYDATE%" == "" FOR /F "TOKENS=1 DELIMS=" %%a IN ('date/t') DO SET MYDATE=%%a
	IF "%MYTIME%" == "" FOR /F "TOKENS=1 DELIMS=" %%a IN ('time/t') DO SET MYTIME=%%a

REM *****************************************************************************************************************
REM Try to find out UTC offset 
REM *****************************************************************************************************************

	REM *** Start with setting a dummy value which is used when we are not able to find out the real UTC offset
	SET UTC=(LOCAL TIME)
	SET UTC_HR=
	SET UTC_MIN=
	SET UTC_SIGN=
	
	REM *** Now get the timezone settings from the registry
	reg export "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\TimeZoneInformation" %TEMP%\TZ-%GENERATED_PROGRAM%.TMP
	REM was: regedit /e %TEMP%\TZ-%GENERATED_PROGRAM%.TMP "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\TimeZoneInformation"
	IF NOT EXIST %TEMP%\TZ-%GENERATED_PROGRAM%.TMP GOTO NOTZINFO

	for /f "Tokens=1* Delims==" %%a in ('type %TEMP%\TZ-%GENERATED_PROGRAM%.TMP') do if %%a == "ActiveTimeBias" SET ACTIVEBIAS=%%b
	for /f "Tokens=1* Delims=:" %%a in ('echo %ACTIVEBIAS%') do ( SET ACTIVEBIAS=%%b & SET PARTYP=%%a )
	
	REM *** Clean up temporary file
	IF EXIST %TEMP%\TZ-%GENERATED_PROGRAM%.TMP DEL %TEMP%\TZ-%GENERATED_PROGRAM%.TMP
	
	REM *** Check if we really got a dword value from the registry ...
	IF NOT "%PARTYP%"=="dword " goto NOTZINFO

	REM *** Check if we are in UTC timezone, then we can avoid some stress...
	if "%ACTIVEBIAS%" == "00000000" SET UTC=(UTC) & GOTO NOTZINFO
	
	SET HI=0x%ACTIVEBIAS:~0,4%
	SET LO=0x%ACTIVEBIAS:~4,4%
	
	if "%HI%"=="0xffff" ( SET /A ACTIVEBIAS=%LO% - %HI% - 1 ) ELSE ( SET /A ACTIVEBIAS=%LO%)
	SET /A UTC_HR="%ACTIVEBIAS%/60"
	SET /A UTC_MIN="%ACTIVEBIAS% %% 60"
	SET UTC_SIGN=%ACTIVEBIAS:~0,1%

	REM *** check the direction in which the local timezone alters UTC time
	IF NOT "%UTC_SIGN%"=="-" SET UTC_SIGN=+
	IF "%UTC_SIGN%"=="-" SET UTC_HR=%UTC_HR:~1,2%

	REM *** Now turn the direction, because we need to know it from the viewpoint of UTC
	IF "%UTC_SIGN%"=="+" (SET UTC_SIGN=-) ELSE (SET UTC_SIGN=+)

	REM *** Put the values in a "00" format
	IF %UTC_HR% LEQ 9 SET UTC_HR=0%UTC_HR%
	IF %UTC_MIN% LEQ 9 SET UTC_MIN=0%UTC_MIN%
			
	REM *** Set up UTC offset string used in version string
	SET UTC=(UTC%UTC_SIGN%%UTC_HR%:%UTC_MIN%)
	
	
:NOTZINFO
echo off

REM *****************************************************************************************************************
REM Now grab the Version number out of the source code (using the packageinfo.sh file...)
REM *****************************************************************************************************************

	REM First, get the main NTP version number. In recent versions this must be extracted 
	REM from a packageinfo.sh file while in earlier versions the info was available from 
	REM a version.m4 file.
	SET F_PACKAGEINFO_SH=..\..\..\..\packageinfo.sh
	IF EXIST %F_PACKAGEINFO_SH% goto VER_FROM_PACKAGE_INFO
	REM next two lines can go away when all windows compilers are building under
	rem ports\winnt\<compiler dir>\<binary name dir> (ports\winnt\vs2008\ntpd)
	rem rather than ports\winnt\<binary name dir> (ports\winnt\ntpd)
	SET F_PACKAGEINFO_SH=..\..\..\packageinfo.sh
	IF EXIST %F_PACKAGEINFO_SH% goto VER_FROM_PACKAGE_INFO
        goto ERRNOVERF

:VER_FROM_PACKAGE_INFO
	REM Get version from packageinfo.sh file, which contains lines reading e.g.
	
	TYPE %F_PACKAGEINFO_SH% | FIND /V "rcpoint=" | FIND /V "betapoint=" | FIND "point=" > point.txt
	SET F_POINT_SH=point.txt
	
	FOR /F "eol=# TOKENS=2 DELIMS==" %%a IN ('findstr  "proto=" %%F_PACKAGEINFO_SH%%') DO SET PROTO=%%a
	FOR /F "eol=# TOKENS=2 DELIMS==" %%a IN ('findstr  "major=" %%F_PACKAGEINFO_SH%%') DO SET MAJOR=%%a
	FOR /F "eol=# TOKENS=2 DELIMS==" %%a IN ('findstr  "minor=" %%F_PACKAGEINFO_SH%%') DO SET MINOR=%%a

	FOR /F "eol=# TOKENS=2 DELIMS==" %%a IN ('findstr  "point=" %%F_POINT_SH%%') DO SET POINT=%%a
	IF "%POINT%"=="NEW" set POINT=
	IF NOT "%POINT%"=="" set POINT=p%POINT%

	FOR /F "eol=# TOKENS=2 DELIMS==" %%a IN ('findstr  "betapoint=" %%F_PACKAGEINFO_SH%%') DO SET BETAPOINT=%%a
	
	FOR /F "eol=# TOKENS=2 DELIMS==" %%a IN ('findstr  "rcpoint=" %%F_PACKAGEINFO_SH%%') DO SET RCPOINT=%%a

	FOR /F "eol=# TOKENS=2 DELIMS==" %%a IN ('findstr  "special=" %%F_PACKAGEINFO_SH%%') DO SET SPECIAL=%%a
	IF NOT "%SPECIAL%"=="" set SPECIAL=-%SPECIAL%

	FOR /F "eol=# TOKENS=2 DELIMS==" %%a IN ('findstr  "prerelease=" %%F_PACKAGEINFO_SH%%') DO SET PRERELEASE=%%a
	IF /I "%PRERELEASE%"=="beta" set PR_SUF=-beta
	IF /I "%PRERELEASE%"=="rc" set PR_SUF=-RC

	FOR /F "eol=# TOKENS=2 DELIMS==" %%a IN ('findstr  "repotype=" %%F_PACKAGEINFO_SH%%') DO SET REPOTYPE=%%a
	IF /I "%REPOTYPE%"=="stable" set REPOTYPE=STABLE
	
	IF NOT "%REPOTYPE%"=="STABLE" SET RCPOINT=
	IF "%PR_SUF%"=="-RC" set PR_POINT=%RCPOINT%
	IF "%PR_SUF%"=="-beta" set PR_POINT=%BETAPOINT%

	SET VER=%PROTO%.%MAJOR%.%MINOR%%POINT%%SPECIAL%%PR_SUF%%PR_POINT%
	
	REM Now we have the version info, try to add a BK ChangeSet version number
	
	REM ** Check if BK is installed ...
	bk -R prs -hr+ -nd:I: ChangeSet 2> NUL > NUL
	IF ERRORLEVEL 1 GOTO NOBK

	REM ** Try to get the CSet rev directly from BK
	FOR /F "TOKENS=1 DELIMS==" %%a IN ('bk.exe -R prs -hr+ -nd:I: ChangeSet') DO @SET CSET=%%a

:NOBK
	REM ** If that was not successful, we'll take a look into a version file, if available
	IF EXIST ..\..\..\..\sntp\scm-rev (
		IF "%CSET%"=="" FOR /F "TOKENS=1" %%a IN ('type ..\..\..\..\sntp\scm-rev') DO @SET CSET=%%a
	)
	REM next if block can go away once all windows compilers are building in
	REM ports\winnt\<compiler dir>\<binary name dir> (ports\winnt\vs2008\ntpd)
	IF EXIST ..\..\..\sntp\scm-rev (
		IF "%CSET%"=="" FOR /F "TOKENS=1" %%a IN ('type ..\..\..\sntp\scm-rev') DO @SET CSET=%%a
	)

	REM ** Now, expand our version number with the CSet revision, if we managed to get one
	IF NOT "%CSET%"=="" SET VER=%VER%@%CSET%
		
	REM We can add a "crypto" identifier (-o) if we see that Crypto support is included in our build
	REM we always include openssl on windows...
	SET VER=%VER%-o


REM *****************************************************************************************************************
REM Check for user settings regarding the time and date format, we use the registry to find out...
REM *****************************************************************************************************************


	REM Any temporary files left from a previous run? Go where you belong...
	IF exist userset.reg del userset.reg
	IF exist userset.txt del userset.txt
	
	reg export "HKEY_CURRENT_USER\Control Panel\International" userset.reg
	REM was: regedit /E userset.reg "HKEY_CURRENT_USER\Control Panel\International"
	IF not exist userset.reg goto ERRNOREG

	rem *** convert from unicode to ascii if necessary
	type userset.reg > userset.txt


	FOR /F "TOKENS=1-9 DELIMS== " %%a IN ('findstr "iDate" userset.txt') DO SET DATEFORMAT=%%b
	FOR /F "TOKENS=1-9 DELIMS== " %%a IN ('findstr "iTime" userset.txt') DO SET TIMEFORMAT=%%b

	FOR /F "TOKENS=1-9 DELIMS== " %%a IN ('findstr /R "sDate\>" userset.txt') DO SET DATEDELIM=%%b
	FOR /F "TOKENS=1-9 DELIMS== " %%a IN ('findstr /R "sTime\>" userset.txt') DO SET TIMEDELIM=%%b
	
	IF "%TIMEFORMAT%"=="" GOTO ERRNOTIME
	IF "%DATEFORMAT%"=="" GOTO ERRNODATE
	IF "%TIMEDELIM%"=="" GOTO ERRNOTIME
	IF "%DATEDELIM%"=="" GOTO ERRNODATE

	SET TIMEDELIM=%TIMEDELIM:~1,1%
	SET DATEDELIM=%DATEDELIM:~1,1%
	SET TIMEFORMAT=%TIMEFORMAT:~1,1%
	SET DATEFORMAT=%DATEFORMAT:~1,1%
	
REM *****************************************************************************************************************
REM Well, well. Its time to look at the time and format it in a standard way (if possible)
REM *****************************************************************************************************************


	FOR /F "TOKENS=1-4 DELIMS=%TIMEDELIM% " %%a IN ('echo %MYTIME%') DO SET AA=%%a&SET BB=%%b&SET CC=%%c&SET DD=%%d

	REM 12H Format
	IF "%TIMEFORMAT%" == "0" (
		SET HOUR=%AA%
		SET MIN=%BB%
		FOR /F "USEBACKQ TOKENS=1 DELIMS=ap" %%a IN ('%BB%') DO SET MMIN=%%a
		SET SEC=%CC%
		SET SUBSEC=%DD%
	)

	REM Get rid of the "a" or "p" if we have one of these in our minute string
	IF NOT "%MMIN%"=="%MIN%" FOR /F "USEBACKQ TOKENS=1 DELIMS=ap " %%a IN ('%MIN%') DO SET MIN=%%a

	REM 24H Format
	IF "%TIMEFORMAT%" == "1" (
		SET HOUR=%AA%
		SET MIN=%BB%
		SET SEC=%CC%
		SET SUBSEC=%DD%
	)

	IF "%HOUR%"=="" GOTO ERRNOTIME
	IF "%MIN%"=="" GOTO ERRNOTIME
	
	IF "%SEC%"=="" SET SEC=00
	IF "%SUBSEC%"=="" SET SUBSEC=00


REM *****************************************************************************************************************
REM It's time to format the date :-)
REM *****************************************************************************************************************


	FOR /F "TOKENS=1-4 DELIMS=./- " %%a IN ('ECHO %MYDATE%') DO SET AA=%%a&SET BB=%%b&SET CC=%%c&SET DD=%%d

	IF "%DD%" == "" (
		REM No Day of Week in Date
		( IF "%DATEFORMAT%" == "0" SET DOW=_&SET DAY=%BB%&SET NMM=%AA%&SET YEAR=%CC% )
		( IF "%DATEFORMAT%" == "1" SET DOW=_&SET DAY=%AA%&SET NMM=%BB%&SET YEAR=%CC% )
		( IF "%DATEFORMAT%" == "2" SET DOW=_&SET DAY=%CC%&SET NMM=%BB%&SET YEAR=%AA% )
	) ELSE (
		( IF "%DATEFORMAT%" == "0" SET DOW=%AA%&SET DAY=%CC%&SET NMM=%BB%&SET YEAR=%DD% )
		( IF "%DATEFORMAT%" == "1" SET DOW=%AA%&SET DAY=%BB%&SET NMM=%CC%&SET YEAR=%DD% )
		( IF "%DATEFORMAT%" == "2" SET DOW=%AA%&SET DAY=%DD%&SET NMM=%CC%&SET YEAR=%BB% )
	)
	
	REM Something went wrong, we weren't able to get a valid date
	IF NOT "%YEAR%" == "0" GOTO DATEOK
	goto ERRNODATE

:DATEOK

	REM Clean up any temporary files we may have created...
	REM IF exist userset.reg del userset.reg
	REM IF exist userset.txt del userset.txt

	IF "%NMM%" == "01" SET MONTH=Jan
	IF "%NMM%" == "02" SET MONTH=Feb
	IF "%NMM%" == "03" SET MONTH=Mar
	IF "%NMM%" == "04" SET MONTH=Apr
	IF "%NMM%" == "05" SET MONTH=May
	IF "%NMM%" == "06" SET MONTH=Jun
	IF "%NMM%" == "07" SET MONTH=Jul
	IF "%NMM%" == "08" SET MONTH=Aug
	IF "%NMM%" == "09" SET MONTH=Sep
	IF "%NMM%" == "10" SET MONTH=Oct
	IF "%NMM%" == "11" SET MONTH=Nov
	IF "%NMM%" == "12" SET MONTH=Dec

	IF NOT {%MONTH%} == {} GOTO DATE_OK

	REM *** Not US date format! Assume ISO: yyyy-mm-dd

	FOR /F "TOKENS=1-4 DELIMS=/- " %%a IN ('date/t') DO SET DAY=%%a&SET yyyy=%%b&SET nmm=%%c&SET dd=%%d

	echo a=%%a b=%%b c=%%c d=%%d
	IF "%NMM%" == "01" SET MONTH=Jan
	IF "%NMM%" == "02" SET MONTH=Feb
	IF "%NMM%" == "03" SET MONTH=Mar
	IF "%NMM%" == "04" SET MONTH=Apr
	IF "%NMM%" == "05" SET MONTH=May
	IF "%NMM%" == "06" SET MONTH=Jun
	IF "%NMM%" == "07" SET MONTH=Jul
	IF "%NMM%" == "08" SET MONTH=Aug
	IF "%NMM%" == "09" SET MONTH=Sep
	IF "%NMM%" == "10" SET MONTH=Oct
	IF "%NMM%" == "11" SET MONTH=Nov
	IF "%NMM%" == "12" SET MONTH=Dec

:DATE_OK
	IF "%SS" == "" SET SS="00"


REM *****************************************************************************************************************
REM Now create a valid version.c file ...
REM *****************************************************************************************************************

	ECHO Version %VER% Build %RUN% date %MONTH%/%DAY%/%YEAR% time %HOUR%:%MIN%:%SEC% %UTC%
	ECHO char * Version = "%GENERATED_PROGRAM% %VER% %MONTH% %DAY% %HOUR%:%MIN%:%SEC% %UTC% %YEAR% (%RUN%)" ; > version.c
	GOTO EOF


REM *****************************************************************************************************************
REM Here are the error messages I know
REM *****************************************************************************************************************
:ERRNOREG
   ECHO "Error: Registry could not be read (check if reg.exe is available and works as expected)"
   GOTO EOF


:ERRNODATE
    ECHO "Error: Dateformat unknown (check if contents of userset.txt are correctly, especially for iDate and sDate)"
	GOTO EOF

:ERRNOTIME
    ECHO "Error: Timeformat unknown (check if contents of userset.txt are correctly, especially for iTime and sTime)"
	GOTO EOF

:ERRNOVERF
    ECHO "Error: Version file not found (searching for ..\..\..\..\packageinfo.sh)"
	GOTO EOF


REM *****************************************************************************************************************
REM Show'em how to run (me)
REM *****************************************************************************************************************
:USAGE

   ECHO Usage: mkver.bat [ -P <Program Name> -H ]
   ECHO   -P          Database Name
   ECHO   -H          Help on options

REM *****************************************************************************************************************
REM All good things come to an end someday. Time to leave
REM *****************************************************************************************************************
:EOF

REM *** Cleaning up 
IF EXIST point.txt DEL point.txt
IF EXIST userset.txt DEL userset.txt
IF EXIST userset.reg DEL userset.reg
