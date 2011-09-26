#
# acpihelp - ACPI Help utility. Displays ASL operator syntax and
# information about ACPI predefined names.
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
PROG = acpihelp

#
# Search paths for source files
#
vpath %.c \
    $(ACPIHELP) \
    $(ACPICA_COMMON)

HEADERS = \
    $(wildcard $(ACPIHELP)/*.h)

OBJECTS = \
	ahamlops.o \
	ahaslkey.o \
	ahaslops.o \
	ahdecode.o \
	ahpredef.o \
	ahmain.o \
	getopt.o

#
# Flags specific to acpihelp
#
CFLAGS+= \
    -DACPI_HELP_APP \
    -I$(ACPIHELP)

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
