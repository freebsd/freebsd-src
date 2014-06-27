#
# acpisrc - ACPICA source code conversion utility
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
FINAL_PROG = ../$(BINDIR)/acpisrc
PROG = $(OBJDIR)/acpisrc

#
# Search path for source files and individual source files
#
vpath %.c \
    $(ACPISRC)\
    $(ACPICA_UTILITIES)\
    $(ACPICA_COMMON)\
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPISRC)/*.h)

OBJECTS = \
    $(OBJDIR)/ascase.o\
    $(OBJDIR)/asconvrt.o\
    $(OBJDIR)/asfile.o\
    $(OBJDIR)/asmain.o\
    $(OBJDIR)/asremove.o\
    $(OBJDIR)/astable.o\
    $(OBJDIR)/asutils.o\
    $(OBJDIR)/cmfsize.o\
    $(OBJDIR)/getopt.o \
    $(OBJDIR)/oslibcfs.o\
    $(OBJDIR)/osunixdir.o\
    $(OBJDIR)/osunixxf.o\
    $(OBJDIR)/utdebug.o\
    $(OBJDIR)/utexcep.o\
    $(OBJDIR)/utglobal.o\
    $(OBJDIR)/utmath.o\
    $(OBJDIR)/utprint.o\
    $(OBJDIR)/utxferror.o

#
# Compile flags specific to acpisrc
#
CFLAGS += \
    -DACPI_SRC_APP\
    -I$(ACPISRC)

#
# Common Rules
#
include ../Makefile.rules
