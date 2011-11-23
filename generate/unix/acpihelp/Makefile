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
PROG = $(OBJDIR)/acpihelp

#
# Search paths for source files
#
vpath %.c \
    $(ACPIHELP) \
    $(ACPICA_COMMON)

HEADERS = \
    $(wildcard $(ACPIHELP)/*.h)

OBJECTS = \
	$(OBJDIR)/ahamlops.o \
	$(OBJDIR)/ahaslkey.o \
	$(OBJDIR)/ahaslops.o \
	$(OBJDIR)/ahdecode.o \
	$(OBJDIR)/ahpredef.o \
	$(OBJDIR)/ahmain.o \
	$(OBJDIR)/getopt.o

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

$(OBJDIR)/%.o : %.c $(HEADERS) $(ACPICA_HEADERS)
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJECTS)

install :
	$(INSTALLPROG)
