@echo off
if "%1" == "h8/300" goto h8300

echo Configuring bfd for go32
update hosts\go32.h sysdep.h
update Makefile.dos Makefile
echo s/@WORDSIZE@/32/g>config.sed
sed -e s/^/s\/@VERSION@\// -e s/$/\/g/g version >>config.sed
sed -f config.sed < bfd-in2.h > bfd.h2
update bfd.h2 bfd.h
goto exit

:h8300
echo Configuring bfd for H8/300
update hosts\h-go32.h sysdep.h
update Makefile.dos Makefile

:exit
