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
	$(OBJDIR)/cmfsize.o\
	$(OBJDIR)/getopt.o\
	$(OBJDIR)/osunixdir.o\
	$(OBJDIR)/osunixmap.o\
	$(OBJDIR)/osunixxf.o\
	$(OBJDIR)/tbprint.o\
	$(OBJDIR)/tbxfroot.o\
	$(OBJDIR)/utascii.o\
	$(OBJDIR)/utbuffer.o\
	$(OBJDIR)/utcksum.o\
	$(OBJDIR)/utdebug.o\
	$(OBJDIR)/utexcep.o\
	$(OBJDIR)/utglobal.o\
	$(OBJDIR)/uthex.o\
	$(OBJDIR)/utmath.o\
	$(OBJDIR)/utnonansi.o\
	$(OBJDIR)/utstring.o\
	$(OBJDIR)/utstrsuppt.o\
	$(OBJDIR)/utstrtoul64.o\
	$(OBJDIR)/utxferror.o

#
# Per-host interfaces
#
ifeq ($(ACPI_HOST), _DragonFly)
HOST_FAMILY = BSD
endif

ifeq ($(ACPI_HOST), _FreeBSD)
HOST_FAMILY = BSD
endif

ifeq ($(ACPI_HOST), _NetBSD)
HOST_FAMILY = BSD
endif

ifeq ($(ACPI_HOST), _QNX)
HOST_FAMILY = BSD
endif

ifeq ($(HOST_FAMILY), BSD)
OBJECTS += \
	$(OBJDIR)/osbsdtbl.o
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
