#
# acpibin - Binary ACPI table utility
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
PROG = acpibin

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
    -DACPI_BIN_APP \
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
    abcompare.o \
    abmain.o \
	utalloc.o \
	utcache.o \
	utdebug.o \
	utdecode.o \
	utglobal.o \
	utlock.o \
	utmath.o \
	utmisc.o \
	utmutex.o \
	utstate.o \
	utxferror.o \
	osunixxf.o \
    getopt.o

#
# Root rule
#
$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(PROG)

#
# acpibin source
#
abcompare.o :       $(ACPICA_TOOLS)/acpibin/abcompare.c
	$(COMPILE)

abmain.o :          $(ACPICA_TOOLS)/acpibin/abmain.c
	$(COMPILE)

#
# ACPICA core source - common
#
getopt.o :          $(ACPICA_COMMON)/getopt.c
	$(COMPILE)

#
# ACPICA core source
#
utalloc.o :         $(ACPICA_CORE)/utilities/utalloc.c
	$(COMPILE)

utcache.o :         $(ACPICA_CORE)/utilities/utcache.c
	$(COMPILE)

utdebug.o :         $(ACPICA_CORE)/utilities/utdebug.c
	$(COMPILE)

utdecode.o :        $(ACPICA_CORE)/utilities/utdecode.c
	$(COMPILE)

utglobal.o :        $(ACPICA_CORE)/utilities/utglobal.c
	$(COMPILE)

utlock.o :          $(ACPICA_CORE)/utilities/utlock.c
	$(COMPILE)

utmath.o :          $(ACPICA_CORE)/utilities/utmath.c
	$(COMPILE)

utmisc.o :          $(ACPICA_CORE)/utilities/utmisc.c
	$(COMPILE)

utmutex.o :         $(ACPICA_CORE)/utilities/utmutex.c
	$(COMPILE)

utstate.o :         $(ACPICA_CORE)/utilities/utstate.c
	$(COMPILE)

utxferror.o :       $(ACPICA_CORE)/utilities/utxferror.c
	$(COMPILE)

#
# Unix OS services layer (OSL)
#
osunixxf.o :        $(ACPICA_OSL)/osunixxf.c
	$(COMPILE)


clean :
	rm -f $(PROG) $(PROG) $(OBJS)

install :
	$(INSTALLPROG)
