if errorlevel 1 type nul
if not exist dup\nul mkdir dup
if errorlevel 1 exit /b

if not errorlevel 1 copy /y ..\ter-*.bdf
if not errorlevel 1 make -B -j8 terminus.fon
if not errorlevel 1 move /y terminus.fon terminus.bak

if not errorlevel 1 call :difftofcp ao2
if not errorlevel 1 call :difftofcp dv1
if not errorlevel 1 call :difftofcp ge2
if not errorlevel 1 call :difftofcp gq2
if not errorlevel 1 call :difftofcp ij1

if not errorlevel 1 copy /y ..\dup\xos4-2.dup dup
if not errorlevel 1 call :difftofcp ka2

if not errorlevel 1 call :difftofcp ll2
if not errorlevel 1 call :difftofcp td1
if not errorlevel 1 call :difftofcp hi2

if not errorlevel 1 move /y terminus.fon terminus.bak
if not errorlevel 1 patch --binary -p1 < ..\alt\hi2.diff
if not errorlevel 1 call :difftofcp hi2-dv1

if not errorlevel 1 patch --binary -p1 < ..\alt\hi2.diff
if not errorlevel 1 copy /y ..\dup\xos4-2.dup dup
if not errorlevel 1 call :difftofcp hi2-ka2

if not errorlevel 1 make -B -j8 terminus.fon
if not errorlevel 1 make fcpw.exe
if not errorlevel 1 del ter-*.bdf ter-*.fnt terminus.bak ter-*.o dup\xos4-2.dup
exit /b

:difftofcp
if not errorlevel 1 patch --binary -p1 < ..\alt\%1.diff
if not errorlevel 1 make -B -j8 fnt
if not errorlevel 1 make terminus.fon
if not errorlevel 1 fc /b terminus.bak terminus.fon > %1.txt
rem fc sets errorlevel 1 on differences
if not errorlevel 2 copy /y ..\ter-*.bdf
