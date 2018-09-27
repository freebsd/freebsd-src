#
# acpiexamples - Example ACPICA initialization code and control
# method execution.
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
FINAL_PROG = ../$(BINDIR)/acpiexamples
PROG = $(OBJDIR)/acpiexamples

#
# Search paths for source files
#
vpath %.c \
    $(ACPIEXAMPLES)\
    $(ACPICA_DISPATCHER)\
    $(ACPICA_EVENTS)\
    $(ACPICA_EXECUTER)\
    $(ACPICA_HARDWARE)\
    $(ACPICA_NAMESPACE)\
    $(ACPICA_PARSER)\
    $(ACPICA_TABLES)\
    $(ACPICA_UTILITIES)\
    $(ACPICA_COMMON)\
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIEXAMPLES)/*.h)

OBJECTS = \
	$(OBJDIR)/examples.o\
	$(OBJDIR)/exstubs.o\
	$(OBJDIR)/extables.o\
	$(OBJDIR)/dsargs.o\
	$(OBJDIR)/dscontrol.o\
	$(OBJDIR)/dsdebug.o\
	$(OBJDIR)/dsfield.o\
	$(OBJDIR)/dsinit.o\
	$(OBJDIR)/dsmethod.o\
	$(OBJDIR)/dsmthdat.o\
	$(OBJDIR)/dsobject.o\
	$(OBJDIR)/dsopcode.o\
	$(OBJDIR)/dspkginit.o\
	$(OBJDIR)/dsutils.o\
	$(OBJDIR)/dswexec.o\
	$(OBJDIR)/dswload.o\
	$(OBJDIR)/dswload2.o\
	$(OBJDIR)/dswscope.o\
	$(OBJDIR)/dswstate.o\
	$(OBJDIR)/evhandler.o\
	$(OBJDIR)/evmisc.o\
	$(OBJDIR)/evregion.o\
	$(OBJDIR)/evrgnini.o\
	$(OBJDIR)/evxface.o\
	$(OBJDIR)/evxfregn.o\
	$(OBJDIR)/exconcat.o\
	$(OBJDIR)/exconfig.o\
	$(OBJDIR)/exconvrt.o\
	$(OBJDIR)/excreate.o\
	$(OBJDIR)/exdebug.o\
	$(OBJDIR)/exdump.o\
	$(OBJDIR)/exfield.o\
	$(OBJDIR)/exfldio.o\
	$(OBJDIR)/exmisc.o\
	$(OBJDIR)/exmutex.o\
	$(OBJDIR)/exnames.o\
	$(OBJDIR)/exoparg1.o\
	$(OBJDIR)/exoparg2.o\
	$(OBJDIR)/exoparg3.o\
	$(OBJDIR)/exoparg6.o\
	$(OBJDIR)/exprep.o\
	$(OBJDIR)/exregion.o\
	$(OBJDIR)/exresnte.o\
	$(OBJDIR)/exresolv.o\
	$(OBJDIR)/exresop.o\
	$(OBJDIR)/exserial.o\
	$(OBJDIR)/exstore.o\
	$(OBJDIR)/exstoren.o\
	$(OBJDIR)/exstorob.o\
	$(OBJDIR)/exsystem.o\
	$(OBJDIR)/extrace.o\
	$(OBJDIR)/exutils.o\
	$(OBJDIR)/hwpci.o\
	$(OBJDIR)/nsaccess.o\
	$(OBJDIR)/nsalloc.o\
	$(OBJDIR)/nsarguments.o\
	$(OBJDIR)/nsconvert.o\
	$(OBJDIR)/nsdump.o\
	$(OBJDIR)/nseval.o\
	$(OBJDIR)/nsinit.o\
	$(OBJDIR)/nsload.o\
	$(OBJDIR)/nsnames.o\
	$(OBJDIR)/nsobject.o\
	$(OBJDIR)/nsparse.o\
	$(OBJDIR)/nspredef.o\
	$(OBJDIR)/nsprepkg.o\
	$(OBJDIR)/nsrepair.o\
	$(OBJDIR)/nsrepair2.o\
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
	$(OBJDIR)/utcopy.o\
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
	$(OBJDIR)/utobject.o\
	$(OBJDIR)/utosi.o\
	$(OBJDIR)/utownerid.o\
	$(OBJDIR)/utnonansi.o\
	$(OBJDIR)/utpredef.o\
	$(OBJDIR)/utresrc.o\
	$(OBJDIR)/utstate.o\
	$(OBJDIR)/utstring.o\
	$(OBJDIR)/utstrsuppt.o\
	$(OBJDIR)/utstrtoul64.o\
	$(OBJDIR)/utxface.o\
	$(OBJDIR)/utxferror.o\
	$(OBJDIR)/utxfinit.o

#
# Flags specific to acpinames utility
#
CFLAGS += \
    -DACPI_EXAMPLE_APP\
    -I$(ACPIEXAMPLES)

#
# Common Rules
#
include ../Makefile.rules
