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
FINAL_PROG = ../$(BINDIR)/acpibin
PROG = $(OBJDIR)/acpibin

#
# Search paths for source files
#
vpath %.c \
    $(ACPIBIN)\
    $(ACPICA_UTILITIES)\
    $(ACPICA_COMMON)\
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIBIN)/*.h)

OBJECTS = \
	$(OBJDIR)/abcompare.o\
	$(OBJDIR)/abmain.o\
	$(OBJDIR)/utalloc.o\
	$(OBJDIR)/utbuffer.o\
	$(OBJDIR)/utcache.o\
	$(OBJDIR)/utdecode.o\
	$(OBJDIR)/utexcep.o\
	$(OBJDIR)/utglobal.o\
	$(OBJDIR)/utlock.o\
	$(OBJDIR)/utmath.o\
	$(OBJDIR)/utmisc.o\
	$(OBJDIR)/utmutex.o\
	$(OBJDIR)/utstate.o\
	$(OBJDIR)/utstring.o\
	$(OBJDIR)/utxferror.o\
	$(OBJDIR)/osunixxf.o\
	$(OBJDIR)/getopt.o

#
# Flags specific to acpibin
#
CFLAGS += \
    -DACPI_BIN_APP\
    -I$(ACPIBIN)

#
# Common Rules
#
include ../Makefile.rules
