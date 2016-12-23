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
FINAL_PROG = ../$(BINDIR)/acpihelp
PROG = $(OBJDIR)/acpihelp

#
# Search paths for source files
#
vpath %.c \
    $(ACPIHELP)\
    $(ACPICA_COMMON)\
    $(ACPICA_UTILITIES)\
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIHELP)/*.h)

OBJECTS = \
	$(OBJDIR)/ahaml.o\
	$(OBJDIR)/ahamlops.o\
	$(OBJDIR)/ahasl.o\
	$(OBJDIR)/ahaslkey.o\
	$(OBJDIR)/ahaslops.o\
	$(OBJDIR)/ahdecode.o\
	$(OBJDIR)/ahgrammar.o\
	$(OBJDIR)/ahids.o\
	$(OBJDIR)/ahpredef.o\
	$(OBJDIR)/ahmain.o\
	$(OBJDIR)/ahtable.o\
	$(OBJDIR)/ahuuids.o\
	$(OBJDIR)/getopt.o\
	$(OBJDIR)/osunixxf.o\
	$(OBJDIR)/utdebug.o\
	$(OBJDIR)/utexcep.o\
	$(OBJDIR)/utglobal.o\
	$(OBJDIR)/uthex.o\
	$(OBJDIR)/utmath.o\
	$(OBJDIR)/utnonansi.o\
	$(OBJDIR)/utpredef.o\
	$(OBJDIR)/utuuid.o

#
# Flags specific to acpihelp
#
CFLAGS += \
    -DACPI_HELP_APP\
    -I$(ACPIHELP)

#
# Common Rules
#
include ../Makefile.rules
