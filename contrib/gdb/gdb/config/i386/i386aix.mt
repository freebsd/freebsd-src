# This port, for aix ps/2 (i386), will allow you to debug the coff
# output generated gcc-2.3.3 + gas.  It will not understand IBM's
# proprietary debug info.
#
# Target: IBM PS/2 (i386) running AIX PS/2
TDEPFILES= i386-tdep.o i387-tdep.o
TM_FILE= tm-i386aix.h
