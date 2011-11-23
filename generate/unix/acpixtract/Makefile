#
# acpixtract - extract binary ACPI tables from acpidump text output
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
PROG = $(OBJDIR)/acpixtract

#
# Search paths for source files
#
vpath %.c \
    $(ACPIXTRACT) \
    $(ACPICA_COMMON)

HEADERS = \
    $(wildcard $(ACPIXTRACT)/*.h)

OBJECTS = \
    $(OBJDIR)/acpixtract.o \
    $(OBJDIR)/axmain.o \
    $(OBJDIR)/getopt.o

#
# Flags specific to acpixtract
#
CFLAGS+= \
    -DACPI_XTRACT_APP \

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
