# Target: AMD 29000
TDEPFILES= a29k-tdep.o remote-eb.o remote-adapt.o
TM_FILE= tm-a29k.h

MT_CFLAGS = -DNO_HIF_SUPPORT
