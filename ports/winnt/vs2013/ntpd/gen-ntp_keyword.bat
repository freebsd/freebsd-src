@echo off
REM gen-ntp_keyword.bat
REM helper to invoke keyword-gen and possibly update ntp_keyword.h
REM Usage:
REM   gen-ntp_keyword dir_containing_keyword-gen.exe
REM
if "%1"=="" goto Usage
if not exist %1\keyword-gen.exe goto ExeNotFound
%1\keyword-gen.exe ..\..\..\..\ntpd\ntp_parser.h > new_keyword.h
findstr /v diff_ignore_line new_keyword.h > new_keyword_cmp.h
findstr /v diff_ignore_line ..\..\..\..\ntpd\ntp_keyword.h > ntp_keyword_cmp.h
set meat_changed=0
fc /L ntp_keyword_cmp.h new_keyword_cmp.h > NUL
if errorlevel 1 set meat_changed=1
del ntp_keyword_cmp.h new_keyword_cmp.h
if "0"=="%meat_changed%" goto SkipUpdate
xcopy /yf new_keyword.h ..\..\..\..\ntpd\ntp_keyword.h
findstr diff_ignore_line new_keyword.h > ..\..\..\..\ntpd\keyword-gen-utd
echo updated keyword-gen-utd and ntp_keyword.h
goto SkipSkipMsg

:skipUpdate
echo ntp_keyword.h is unchanged

:SkipSkipMsg
set meat_changed=
del new_keyword.h
goto Exit

:Usage
echo Usage:
echo   gen-ntp_keyword dir_containing_keyword-gen.exe
goto Exit

:ExeNotFound
echo keyword-gen.exe not found at %1\keyword-gen.exe
goto Exit

:Exit
