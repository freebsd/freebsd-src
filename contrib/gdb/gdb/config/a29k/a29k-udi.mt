# Target: AMD 29000 on EB29K board over a serial line
TDEPFILES= a29k-tdep.o remote-udi.o udip2soc.o udr.o udi2go32.o
TM_FILE= tm-a29k.h

# Disable standard remote support.
REMOTE_OBS=

MT_CFLAGS = $(HOST_IPC)
