@echo off
setlocal
call "%VS90COMNTOOLS%"\vsvars32.bat
pushd "%1"
copy config\win32 config.h
nmake -f win32\makefile.win32 prebuild
popd
