#
# acpisrc - ACPICA source code conversion utility
#
# NOTE: This makefile is intended to be used in the Linux environment,
# with the Linux directory structure. It will not work directly
# on the native ACPICA source tree.
#

#
# Configuration
# Notes:
#   gcc should be version 4 or greater, otherwise some of the options
#       used will not be recognized.
#   Global optimization flags (such as -O2, -Os) are not used, since
#       they cause issues on some compilers.
#   The _GNU_SOURCE symbol is required for many hosts.
#
PROG = acpisrc

HOST =          _LINUX
NOMAN =         YES
COMPILE =       $(CC) -c $(CFLAGS) $(CWARNINGFLAGS) -o$@ $?

ACPICA_COMPONENTS =
ACPICA_SRC =    ../..
ACPICA_COMMON = $(ACPICA_SRC)/common
ACPICA_CORE =   $(ACPICA_SRC)$(ACPICA_COMPONENTS)
ACPICA_TOOLS =  $(ACPICA_SRC)/tools
ACPICA_OSL =    $(ACPICA_SRC)/os_specific/service_layers
INSTALLDIR =    /usr/bin
INSTALLPROG =   cp --remove-destination $(PROG) $(INSTALLDIR)

CFLAGS+= \
    -D$(HOST) \
    -D_GNU_SOURCE \
    -DACPI_SRC_APP \
    -I$(ACPICA_SRC)/include \
    -I.

CWARNINGFLAGS = \
    -ansi \
    -Wall \
    -Wbad-function-cast \
    -Wdeclaration-after-statement \
    -Werror \
    -Wformat=2 \
    -Wmissing-declarations \
    -Wmissing-prototypes \
    -Wstrict-aliasing=0 \
    -Wstrict-prototypes \
    -Wswitch-default \
    -Wpointer-arith \
    -Wundef

OBJS = \
    ascase.o \
    asconvrt.o \
    asfile.o \
    asmain.o \
    asremove.o \
    astable.o \
    asutils.o \
    osunixdir.o \
    getopt.o

#
# Root rule
#
$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(PROG)

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
	rm -f $(PROG) $(PROG) $(OBJS)

install :
	$(INSTALLPROG)
