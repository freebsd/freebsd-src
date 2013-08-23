#
# acpidump - ACPI table dump utility (binary to ascii hex)
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
FINAL_PROG = ../$(BINDIR)/acpidump
PROG = $(OBJDIR)/acpidump

#
# Search paths for source files
#
vpath %.c \
    $(ACPIDUMP)\
    $(ACPICA_TABLES)\
    $(ACPICA_UTILITIES)\
    $(ACPICA_COMMON)\
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIDUMP)/*.h)

OBJECTS = \
	$(OBJDIR)/apdump.o\
	$(OBJDIR)/apfiles.o\
	$(OBJDIR)/apmain.o\
	$(OBJDIR)/osunixdir.o\
	$(OBJDIR)/osunixmap.o\
	$(OBJDIR)/tbprint.o\
	$(OBJDIR)/tbxfroot.o\
	$(OBJDIR)/utbuffer.o\
	$(OBJDIR)/utexcep.o\
	$(OBJDIR)/utmath.o\
	$(OBJDIR)/utstring.o\
	$(OBJDIR)/utxferror.o\
	$(OBJDIR)/getopt.o

#
# Per-host interfaces
#
ifeq ($(HOST), _FreeBSD)
OBJECTS += \
	$(OBJDIR)/osfreebsdtbl.o
else
OBJECTS += \
	$(OBJDIR)/oslinuxtbl.o
endif

#
# Flags specific to acpidump
#
CFLAGS += \
    -DACPI_DUMP_APP\
    -I$(ACPIDUMP)

#
# Common Rules
#
include ../Makefile.rules
