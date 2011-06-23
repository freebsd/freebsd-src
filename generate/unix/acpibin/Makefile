#
# acpibin - Binary ACPI table utility
#

#
# Note: This makefile is intended to be used from within the native
# ACPICA directory structure, from under generate/unix. It specifically
# places all object files in a generate/unix subdirectory, not within
# the various ACPICA source directories. This prevents collisions
# between different compilations of the same source file with different
# compile options, and prevents pollution of the source code.
#
include ../Makefile.config
PROG = acpibin

#
# Flags specific to acpibin
#
CFLAGS+= \
    -DACPI_BIN_APP \
    -I$(ACPICA_TOOLS)/acpibin

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
	$(COPYPROG)

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
	rm -f $(PROG) $(PROG).exe $(OBJS)

install :
	$(INSTALLPROG)
