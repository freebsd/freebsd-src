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
# Search paths for source files
#
vpath %.c \
    $(ACPIBIN) \
    $(ACPICA_UTILITIES) \
    $(ACPICA_COMMON) \
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIBIN)/*.h)

OBJECTS = \
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
# Flags specific to acpibin
#
CFLAGS+= \
    -DACPI_BIN_APP \
    -I$(ACPIBIN)

#
# Rules
#
$(PROG) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(PROG)
	$(COPYPROG)

%.o : %.c $(HEADERS) $(ACPICA_HEADERS)
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJECTS)

install :
	$(INSTALLPROG)
