#
# acpihelp - ACPI Help utility. Displays ASL operator syntax and
# information about ACPI predefined names.
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
PROG = acpihelp

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
    -DACPI_HELP_APP \
    -I$(ACPICA_SRC)/include

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
	ahamlops.o \
	ahaslkey.o \
	ahaslops.o \
	ahdecode.o \
	ahpredef.o \
	ahmain.o \
	getopt.o

#
# Root rule
#
$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(PROG)

#
# acpihelp source
#
ahamlops.o :        $(ACPICA_TOOLS)/acpihelp/ahamlops.c
	$(COMPILE)

ahaslkey.o :        $(ACPICA_TOOLS)/acpihelp/ahaslkey.c
	$(COMPILE)

ahaslops.o :        $(ACPICA_TOOLS)/acpihelp/ahaslops.c
	$(COMPILE)

ahdecode.o :        $(ACPICA_TOOLS)/acpihelp/ahdecode.c
	$(COMPILE)

ahpredef.o :        $(ACPICA_TOOLS)/acpihelp/ahpredef.c
	$(COMPILE)

ahmain.o :          $(ACPICA_TOOLS)/acpihelp/ahmain.c
	$(COMPILE)

#
# ACPICA core source - common
#
getopt.o :          $(ACPICA_COMMON)/getopt.c
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJS)

install :
	$(INSTALLPROG)
