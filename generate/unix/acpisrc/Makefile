#
# acpisrc utility
#
include ../Makefile.config

PROG = acpisrc


#
# Flags specific to acpisrc
#
CFLAGS+= -DACPI_APPLICATION -I$(ACPICA_TOOLS)/acpisrc

OBJS = \
	ascase.o \
	asconvrt.o \
	asfile.o \
	asmain.o \
	asremove.o \
	astable.o \
	asutils.o \
	getopt.o \
	osunixdir.o 


#
# Root rule
#
$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(PROG)
	$(COPYPROG)


#
# acpisrc source
#
ascase.o :          $(ACPICA_TOOLS)/acpisrc/ascase.c
	$(COMPILE)

asconvrt.o :        $(ACPICA_TOOLS)/acpisrc/asconvrt.c
	$(COMPILE)

asfile.o :          $(ACPICA_TOOLS)/acpisrc/asfile.c
	$(COMPILE)

asmain.o :          $(ACPICA_TOOLS)/acpisrc/asmain.c
	$(COMPILE)

asremove.o :        $(ACPICA_TOOLS)/acpisrc/asremove.c
	$(COMPILE)

astable.o :         $(ACPICA_TOOLS)/acpisrc/astable.c
	$(COMPILE)

asutils.o :         $(ACPICA_TOOLS)/acpisrc/asutils.c
	$(COMPILE)


#
# ACPICA core source - common
#
getopt.o :          $(ACPICA_COMMON)/getopt.c
	$(COMPILE)


#
# Unix OS services layer (OSL)
#
osunixdir.o :       $(ACPICA_OSL)/osunixdir.c
	$(COMPILE)


clean : 
	rm -f $(PROG) $(PROG).exe $(OBJS)

install : 
	$(INSTALLPROG)
