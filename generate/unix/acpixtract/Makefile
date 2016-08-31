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
FINAL_PROG = ../$(BINDIR)/acpixtract
PROG = $(OBJDIR)/acpixtract

#
# Search paths for source files
#
vpath %.c \
    $(ACPIXTRACT)\
    $(ACPICA_UTILITIES)\
    $(ACPICA_COMMON)\
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIXTRACT)/*.h)

OBJECTS = \
    $(OBJDIR)/acpixtract.o\
    $(OBJDIR)/axmain.o\
    $(OBJDIR)/axutils.o\
    $(OBJDIR)/getopt.o\
    $(OBJDIR)/osunixxf.o\
    $(OBJDIR)/utascii.o\
    $(OBJDIR)/utdebug.o\
    $(OBJDIR)/utexcep.o\
    $(OBJDIR)/utglobal.o\
	$(OBJDIR)/uthex.o\
    $(OBJDIR)/utmath.o\
    $(OBJDIR)/utnonansi.o\
    $(OBJDIR)/utxferror.o

#
# Flags specific to acpixtract
#
CFLAGS += \
    -DACPI_XTRACT_APP

#
# Common Rules
#
include ../Makefile.rules
