@echo off
echo Configuring GAS for H8/300

copy config\ho-go32.h host.h
copy config\tc-h8300.c targ-cpu.c
copy config\tc-h8300.h targ-cpu.h
copy config\te-generic.h targ-env.h
copy config\objcoff-bfd.h obj-format.h
copy config\objcoff-bfd.c obj-format.c
copy config\atof-ieee.c atof-targ.c

copy Makefile.dos Makefile


