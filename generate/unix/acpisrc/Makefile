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
PROG = acpisrc

#
# Search path for source files and individual source files
#
vpath %.c \
    $(ACPISRC) \
    $(ACPICA_COMMON) \
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPISRC)/*.h)

OBJECTS = \
    ascase.o \
    asconvrt.o \
    asfile.o \
    asmain.o \
    asremove.o \
    astable.o \
    asutils.o \
    osunixdir.o \
    getopt.o

#
# Compile flags specific to acpisrc
#
CFLAGS+= \
    -DACPI_SRC_APP \
    -I$(ACPISRC)

#
# Rules
#
$(PROG) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(PROG)
	$(COPYPROG)

%.o : %.c $(HEADERS) $(ACPICA_HEADERS)
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJECTS)

install :
	$(INSTALLPROG)
