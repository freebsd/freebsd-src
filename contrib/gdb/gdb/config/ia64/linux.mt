# Target: Intel IA-64 running GNU/Linux
TDEPFILES= ia64-tdep.o ia64-aix-tdep.o ia64-linux-tdep.o \
	solib.o solib-svr4.o solib-legacy.o
TM_FILE= tm-linux.h

GDBSERVER_DEPFILES = linux-low.o linux-ia64-low.o reg-ia64.o
