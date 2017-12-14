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
FINAL_PROG = ../$(BINDIR)/acpinames
PROG = $(OBJDIR)/acpinames

#
# Search paths for source files
#
vpath %.c \
    $(ACPINAMES)\
    $(ACPICA_DISPATCHER)\
    $(ACPICA_EXECUTER)\
    $(ACPICA_NAMESPACE)\
    $(ACPICA_PARSER)\
    $(ACPICA_TABLES)\
    $(ACPICA_UTILITIES)\
    $(ACPICA_COMMON)\
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPINAMES)/*.h)

OBJECTS = \
	$(OBJDIR)/acfileio.o\
	$(OBJDIR)/anmain.o\
	$(OBJDIR)/anstubs.o\
	$(OBJDIR)/antables.o\
	$(OBJDIR)/cmfsize.o\
	$(OBJDIR)/dsfield.o\
	$(OBJDIR)/dsinit.o\
	$(OBJDIR)/dsmthdat.o\
	$(OBJDIR)/dsobject.o\
	$(OBJDIR)/dspkginit.o\
	$(OBJDIR)/dsutils.o\
	$(OBJDIR)/dswload.o\
	$(OBJDIR)/dswload2.o\
	$(OBJDIR)/dswscope.o\
	$(OBJDIR)/dswstate.o\
	$(OBJDIR)/excreate.o\
	$(OBJDIR)/exdump.o\
	$(OBJDIR)/exmutex.o\
	$(OBJDIR)/exnames.o\
	$(OBJDIR)/exprep.o\
	$(OBJDIR)/exresnte.o\
	$(OBJDIR)/exresolv.o\
	$(OBJDIR)/exsystem.o\
	$(OBJDIR)/exutils.o\
	$(OBJDIR)/getopt.o\
	$(OBJDIR)/nsaccess.o\
	$(OBJDIR)/nsalloc.o\
	$(OBJDIR)/nsdump.o\
	$(OBJDIR)/nsinit.o\
	$(OBJDIR)/nsload.o\
	$(OBJDIR)/nsnames.o\
	$(OBJDIR)/nsobject.o\
	$(OBJDIR)/nsparse.o\
	$(OBJDIR)/nssearch.o\
	$(OBJDIR)/nsutils.o\
	$(OBJDIR)/nswalk.o\
	$(OBJDIR)/nsxfeval.o\
	$(OBJDIR)/nsxfname.o\
	$(OBJDIR)/nsxfobj.o\
	$(OBJDIR)/osunixxf.o\
	$(OBJDIR)/psargs.o\
	$(OBJDIR)/psloop.o\
	$(OBJDIR)/psobject.o\
	$(OBJDIR)/psopcode.o\
	$(OBJDIR)/psopinfo.o\
	$(OBJDIR)/psparse.o\
	$(OBJDIR)/psscope.o\
	$(OBJDIR)/pstree.o\
	$(OBJDIR)/psutils.o\
	$(OBJDIR)/pswalk.o\
	$(OBJDIR)/psxface.o\
	$(OBJDIR)/tbdata.o\
	$(OBJDIR)/tbfadt.o\
	$(OBJDIR)/tbfind.o\
	$(OBJDIR)/tbinstal.o\
	$(OBJDIR)/tbprint.o\
	$(OBJDIR)/tbutils.o\
	$(OBJDIR)/tbxface.o\
	$(OBJDIR)/tbxfload.o\
	$(OBJDIR)/tbxfroot.o\
	$(OBJDIR)/utaddress.o\
	$(OBJDIR)/utalloc.o\
	$(OBJDIR)/utascii.o\
	$(OBJDIR)/utbuffer.o\
	$(OBJDIR)/utcache.o\
	$(OBJDIR)/utdebug.o\
	$(OBJDIR)/utdecode.o\
	$(OBJDIR)/utdelete.o\
	$(OBJDIR)/uterror.o\
	$(OBJDIR)/uteval.o\
	$(OBJDIR)/utexcep.o\
	$(OBJDIR)/utglobal.o\
	$(OBJDIR)/uthex.o\
	$(OBJDIR)/utids.o\
	$(OBJDIR)/utinit.o\
	$(OBJDIR)/utlock.o\
	$(OBJDIR)/utmath.o\
	$(OBJDIR)/utmisc.o\
	$(OBJDIR)/utmutex.o\
	$(OBJDIR)/utnonansi.o\
	$(OBJDIR)/utobject.o\
	$(OBJDIR)/utosi.o\
	$(OBJDIR)/utownerid.o\
	$(OBJDIR)/utstate.o\
	$(OBJDIR)/utstring.o\
	$(OBJDIR)/utxface.o\
	$(OBJDIR)/utxferror.o\
	$(OBJDIR)/utxfinit.o

#
# Flags specific to acpinames utility
#
CFLAGS += \
    -DACPI_NAMES_APP\
    -I$(ACPINAMES)

#
# Common Rules
#
include ../Makefile.rules
