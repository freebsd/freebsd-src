@echo off
if not exist ter-font.rc exit /b 1
if not exist ter-main.c exit /b 1
echo rm -f *.bdf *.fnt *.txt ter-*.o terminus.fon fcpw.exe
for %%i in (*.bdf *.fnt *.txt ter-*.o terminus.fon fcpw.exe) do if exist %%i del /q %%i
