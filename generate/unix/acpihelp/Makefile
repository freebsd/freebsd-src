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
# Flags specific to acpihelp
#
CFLAGS+= \
    -DACPI_HELP_APP \
    -I$(ACPICA_TOOLS)/acpihelp

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
	$(COPYPROG)

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
