# Target: Intel 386 with a.out in osf 1/mk
TDEPFILES= i386-tdep.o 
TM_FILE= tm-i386osf1mk.h
TM_CFLAGS= -I/usr/mach3/include
TM_CLIBS= /usr/mach3/ccs/lib/libmachid.a /usr/mach3/ccs/lib/libnetname.a /usr/mach3/ccs/lib/libmach.a
OBJFORMATS= dbxread.o
