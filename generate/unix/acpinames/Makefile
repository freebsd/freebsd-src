#
# acpinames - Load ACPI table and dump namespace. This is a subset
# of the AcpiExec functionality, it is intended to demonstrate
# the configurability of ACPICA.
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
PROG = acpinames

#
# Search paths for source files
#
vpath %.c \
    $(ACPINAMES) \
    $(ACPICA_DEBUGGER) \
    $(ACPICA_DISPATCHER) \
    $(ACPICA_EXECUTER) \
    $(ACPICA_NAMESPACE) \
    $(ACPICA_PARSER) \
    $(ACPICA_TABLES) \
    $(ACPICA_UTILITIES) \
    $(ACPICA_COMMON) \
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPINAMES)/*.h)

OBJECTS = \
	anmain.o \
	anstubs.o \
	antables.o \
	dbfileio.o \
	dsfield.o \
	dsmthdat.o \
	dsobject.o \
	dsutils.o \
	dswload.o \
	dswload2.o \
	dswscope.o \
	dswstate.o \
	excreate.o \
	exnames.o \
	exresnte.o \
	exresolv.o \
	exutils.o \
	getopt.o \
	nsaccess.o \
	nsalloc.o \
	nsdump.o \
	nsinit.o \
	nsload.o \
	nsnames.o \
	nsobject.o \
	nsparse.o \
	nssearch.o \
	nsutils.o \
	nswalk.o \
	nsxfeval.o \
	nsxfname.o \
	nsxfobj.o \
	osunixxf.o \
	psargs.o \
	psloop.o \
	psopcode.o \
	psparse.o \
	psscope.o \
	pstree.o \
	psutils.o \
	pswalk.o \
	psxface.o \
	tbfadt.o \
	tbfind.o \
	tbinstal.o \
	tbutils.o \
	tbxface.o \
	tbxfroot.o \
	utalloc.o \
	utcache.o \
	utdebug.o \
	utdecode.o \
	utdelete.o \
	utglobal.o \
	utlock.o \
	utmath.o \
	utmisc.o \
	utmutex.o \
	utobject.o \
	utstate.o \
	utosi.o \
	utxferror.o \
	utxface.o

#
# Flags specific to acpinames utility
#
CFLAGS+= \
    -DACPI_NAMES_APP \
    -I$(ACPINAMES)

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
