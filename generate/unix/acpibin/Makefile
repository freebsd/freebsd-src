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
PROG = $(OBJDIR)/acpibin

#
# Search paths for source files
#
vpath %.c \
    $(ACPIBIN) \
    $(ACPICA_UTILITIES) \
    $(ACPICA_COMMON) \
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIBIN)/*.h)

OBJECTS = \
   	$(OBJDIR)/abcompare.o \
	$(OBJDIR)/abmain.o \
	$(OBJDIR)/utalloc.o \
	$(OBJDIR)/utcache.o \
	$(OBJDIR)/utdebug.o \
	$(OBJDIR)/utdecode.o \
	$(OBJDIR)/utglobal.o \
	$(OBJDIR)/utlock.o \
	$(OBJDIR)/utmath.o \
	$(OBJDIR)/utmisc.o \
	$(OBJDIR)/utmutex.o \
	$(OBJDIR)/utstate.o \
	$(OBJDIR)/utxferror.o \
	$(OBJDIR)/osunixxf.o \
	$(OBJDIR)/getopt.o

#
# Flags specific to acpibin
#
CFLAGS+= \
    -DACPI_BIN_APP \
    -I$(ACPIBIN)

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
