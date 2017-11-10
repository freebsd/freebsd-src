#
# acpiexec: ACPI execution simulator. Runs ACPICA code in user
# space. Loads ACPI tables, displays the namespace, and allows
# execution of control methods.
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
FINAL_PROG = ../$(BINDIR)/acpiexec
PROG = $(OBJDIR)/acpiexec

#
# Search paths for source files
#
vpath %.c \
    $(ACPIEXEC)\
    $(ACPICA_DEBUGGER)\
    $(ACPICA_DISASSEMBLER)\
    $(ACPICA_DISPATCHER)\
    $(ACPICA_EVENTS)\
    $(ACPICA_EXECUTER)\
    $(ACPICA_HARDWARE)\
    $(ACPICA_NAMESPACE)\
    $(ACPICA_PARSER)\
    $(ACPICA_RESOURCES)\
    $(ACPICA_TABLES)\
    $(ACPICA_UTILITIES)\
    $(ACPICA_COMMON)\
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIEXEC)/*.h)

OBJECTS = \
	$(OBJDIR)/acgetline.o\
	$(OBJDIR)/acfileio.o\
	$(OBJDIR)/aeexec.o\
	$(OBJDIR)/aeexception.o\
	$(OBJDIR)/aehandlers.o\
	$(OBJDIR)/aeinitfile.o\
	$(OBJDIR)/aeinstall.o\
	$(OBJDIR)/aemain.o\
	$(OBJDIR)/aeregion.o\
	$(OBJDIR)/aetables.o\
	$(OBJDIR)/aetests.o\
	$(OBJDIR)/ahids.o\
	$(OBJDIR)/ahuuids.o\
	$(OBJDIR)/cmfsize.o\
	$(OBJDIR)/dbcmds.o\
	$(OBJDIR)/dbconvert.o\
	$(OBJDIR)/dbdisply.o\
	$(OBJDIR)/dbexec.o\
	$(OBJDIR)/dbfileio.o\
	$(OBJDIR)/dbhistry.o\
	$(OBJDIR)/dbinput.o\
	$(OBJDIR)/dbmethod.o\
	$(OBJDIR)/dbnames.o\
	$(OBJDIR)/dbobject.o\
	$(OBJDIR)/dbstats.o\
	$(OBJDIR)/dbtest.o\
	$(OBJDIR)/dbutils.o\
	$(OBJDIR)/dbxface.o\
	$(OBJDIR)/dmbuffer.o\
	$(OBJDIR)/dmcstyle.o\
	$(OBJDIR)/dmdeferred.o\
	$(OBJDIR)/dmnames.o\
	$(OBJDIR)/dmopcode.o\
	$(OBJDIR)/dmresrc.o\
	$(OBJDIR)/dmresrcl.o\
	$(OBJDIR)/dmresrcl2.o\
	$(OBJDIR)/dmresrcs.o\
	$(OBJDIR)/dmutils.o\
	$(OBJDIR)/dmwalk.o\
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
	$(OBJDIR)/evevent.o\
	$(OBJDIR)/evglock.o\
	$(OBJDIR)/evgpe.o\
	$(OBJDIR)/evgpeblk.o\
	$(OBJDIR)/evgpeinit.o\
	$(OBJDIR)/evgpeutil.o\
	$(OBJDIR)/evhandler.o\
	$(OBJDIR)/evmisc.o\
	$(OBJDIR)/evregion.o\
	$(OBJDIR)/evrgnini.o\
	$(OBJDIR)/evsci.o\
	$(OBJDIR)/evxface.o\
	$(OBJDIR)/evxfevnt.o\
	$(OBJDIR)/evxfgpe.o\
	$(OBJDIR)/evxfregn.o\
	$(OBJDIR)/exconfig.o\
	$(OBJDIR)/exconcat.o\
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
	$(OBJDIR)/exstore.o\
	$(OBJDIR)/exstoren.o\
	$(OBJDIR)/exstorob.o\
	$(OBJDIR)/exsystem.o\
	$(OBJDIR)/extrace.o\
	$(OBJDIR)/exutils.o\
	$(OBJDIR)/getopt.o\
	$(OBJDIR)/hwacpi.o\
	$(OBJDIR)/hwesleep.o\
	$(OBJDIR)/hwgpe.o\
	$(OBJDIR)/hwpci.o\
	$(OBJDIR)/hwregs.o\
	$(OBJDIR)/hwsleep.o\
	$(OBJDIR)/hwtimer.o\
	$(OBJDIR)/hwvalid.o\
	$(OBJDIR)/hwxface.o\
	$(OBJDIR)/hwxfsleep.o\
	$(OBJDIR)/nsaccess.o\
	$(OBJDIR)/nsalloc.o\
	$(OBJDIR)/nsarguments.o\
	$(OBJDIR)/nsconvert.o\
	$(OBJDIR)/nsdump.o\
	$(OBJDIR)/nsdumpdv.o\
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
	$(OBJDIR)/osgendbg.o\
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
	$(OBJDIR)/rsaddr.o\
	$(OBJDIR)/rscalc.o\
	$(OBJDIR)/rscreate.o\
	$(OBJDIR)/rsdump.o\
	$(OBJDIR)/rsdumpinfo.o\
	$(OBJDIR)/rsinfo.o\
	$(OBJDIR)/rsio.o\
	$(OBJDIR)/rsirq.o\
	$(OBJDIR)/rslist.o\
	$(OBJDIR)/rsmemory.o\
	$(OBJDIR)/rsmisc.o\
	$(OBJDIR)/rsserial.o\
	$(OBJDIR)/rsutils.o\
	$(OBJDIR)/rsxface.o\
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
	$(OBJDIR)/utresdecode.o\
	$(OBJDIR)/utresrc.o\
	$(OBJDIR)/utstate.o\
	$(OBJDIR)/utstring.o\
	$(OBJDIR)/utstrsuppt.o\
	$(OBJDIR)/utstrtoul64.o\
	$(OBJDIR)/uttrack.o\
	$(OBJDIR)/utuuid.o\
	$(OBJDIR)/utxface.o\
	$(OBJDIR)/utxferror.o\
	$(OBJDIR)/utxfinit.o\
	$(OBJDIR)/utxfmutex.o

#
# Flags specific to acpiexec utility
#
CFLAGS += \
    -DACPI_EXEC_APP\
    -I$(ACPIEXEC)

ifeq ($(ASLTS),TRUE)
CFLAGS += \
    -DACPI_CHECKSUM_ABORT=TRUE
endif

ifneq ($(HOST),_QNX)
LDFLAGS += -lpthread
endif

ifneq ($(HOST),_APPLE)
ifneq ($(HOST),_QNX)
LDFLAGS += -lrt
endif
endif

#
# Common Rules
#
include ../Makefile.rules
