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
PROG = $(OBJDIR)/acpiexec

#
# Search paths for source files
#
vpath %.c \
    $(ACPIEXEC) \
    $(ACPICA_DEBUGGER) \
    $(ACPICA_DISASSEMBLER) \
    $(ACPICA_DISPATCHER) \
    $(ACPICA_EVENTS) \
    $(ACPICA_EXECUTER) \
    $(ACPICA_HARDWARE) \
    $(ACPICA_NAMESPACE) \
    $(ACPICA_PARSER) \
    $(ACPICA_RESOURCES) \
    $(ACPICA_TABLES) \
    $(ACPICA_UTILITIES) \
    $(ACPICA_COMMON) \
    $(ACPICA_OSL)

HEADERS = \
    $(wildcard $(ACPIEXEC)/*.h)

OBJECTS = \
	$(OBJDIR)/aeexec.o \
	$(OBJDIR)/aehandlers.o \
	$(OBJDIR)/aemain.o \
	$(OBJDIR)/aetables.o \
	$(OBJDIR)/dbcmds.o \
	$(OBJDIR)/dbdisply.o \
	$(OBJDIR)/dbexec.o \
	$(OBJDIR)/dbfileio.o \
	$(OBJDIR)/dbhistry.o \
	$(OBJDIR)/dbinput.o \
	$(OBJDIR)/dbmethod.o \
	$(OBJDIR)/dbnames.o \
	$(OBJDIR)/dbstats.o \
	$(OBJDIR)/dbutils.o \
	$(OBJDIR)/dbxface.o \
	$(OBJDIR)/dmbuffer.o \
	$(OBJDIR)/dmnames.o \
	$(OBJDIR)/dmobject.o \
	$(OBJDIR)/dmopcode.o \
	$(OBJDIR)/dmresrc.o \
	$(OBJDIR)/dmresrcl.o \
	$(OBJDIR)/dmresrcl2.o \
	$(OBJDIR)/dmresrcs.o \
	$(OBJDIR)/dmutils.o \
	$(OBJDIR)/dmwalk.o \
	$(OBJDIR)/dsargs.o \
	$(OBJDIR)/dscontrol.o \
	$(OBJDIR)/dsfield.o \
	$(OBJDIR)/dsinit.o \
	$(OBJDIR)/dsmethod.o \
	$(OBJDIR)/dsmthdat.o \
	$(OBJDIR)/dsobject.o \
	$(OBJDIR)/dsopcode.o \
	$(OBJDIR)/dsutils.o \
	$(OBJDIR)/dswexec.o \
	$(OBJDIR)/dswload.o \
	$(OBJDIR)/dswload2.o \
	$(OBJDIR)/dswscope.o \
	$(OBJDIR)/dswstate.o \
	$(OBJDIR)/evevent.o \
	$(OBJDIR)/evglock.o \
	$(OBJDIR)/evgpe.o \
	$(OBJDIR)/evgpeblk.o \
	$(OBJDIR)/evgpeinit.o \
	$(OBJDIR)/evgpeutil.o \
	$(OBJDIR)/evmisc.o \
	$(OBJDIR)/evregion.o \
	$(OBJDIR)/evrgnini.o \
	$(OBJDIR)/evsci.o \
	$(OBJDIR)/evxface.o \
	$(OBJDIR)/evxfevnt.o \
	$(OBJDIR)/evxfgpe.o \
	$(OBJDIR)/evxfregn.o \
	$(OBJDIR)/exconfig.o \
	$(OBJDIR)/exconvrt.o \
	$(OBJDIR)/excreate.o \
	$(OBJDIR)/exdebug.o \
	$(OBJDIR)/exdump.o \
	$(OBJDIR)/exfield.o \
	$(OBJDIR)/exfldio.o \
	$(OBJDIR)/exmisc.o \
	$(OBJDIR)/exmutex.o \
	$(OBJDIR)/exnames.o \
	$(OBJDIR)/exoparg1.o \
	$(OBJDIR)/exoparg2.o \
	$(OBJDIR)/exoparg3.o \
	$(OBJDIR)/exoparg6.o \
	$(OBJDIR)/exprep.o \
	$(OBJDIR)/exregion.o \
	$(OBJDIR)/exresnte.o \
	$(OBJDIR)/exresolv.o \
	$(OBJDIR)/exresop.o \
	$(OBJDIR)/exstore.o \
	$(OBJDIR)/exstoren.o \
	$(OBJDIR)/exstorob.o \
	$(OBJDIR)/exsystem.o \
	$(OBJDIR)/exutils.o \
	$(OBJDIR)/getopt.o \
	$(OBJDIR)/hwacpi.o \
	$(OBJDIR)/hwesleep.o \
	$(OBJDIR)/hwgpe.o \
	$(OBJDIR)/hwpci.o \
	$(OBJDIR)/hwregs.o \
	$(OBJDIR)/hwsleep.o \
	$(OBJDIR)/hwvalid.o \
	$(OBJDIR)/hwxface.o \
	$(OBJDIR)/hwxfsleep.o \
	$(OBJDIR)/nsaccess.o \
	$(OBJDIR)/nsalloc.o \
	$(OBJDIR)/nsdump.o \
	$(OBJDIR)/nsdumpdv.o \
	$(OBJDIR)/nseval.o \
	$(OBJDIR)/nsinit.o \
	$(OBJDIR)/nsload.o \
	$(OBJDIR)/nsnames.o \
	$(OBJDIR)/nsobject.o \
	$(OBJDIR)/nsparse.o \
	$(OBJDIR)/nspredef.o \
	$(OBJDIR)/nsrepair.o \
	$(OBJDIR)/nsrepair2.o \
	$(OBJDIR)/nssearch.o \
	$(OBJDIR)/nsutils.o \
	$(OBJDIR)/nswalk.o \
	$(OBJDIR)/nsxfeval.o \
	$(OBJDIR)/nsxfname.o \
	$(OBJDIR)/nsxfobj.o \
	$(OBJDIR)/osunixxf.o \
	$(OBJDIR)/psargs.o \
	$(OBJDIR)/psloop.o \
	$(OBJDIR)/psopcode.o \
	$(OBJDIR)/psparse.o \
	$(OBJDIR)/psscope.o \
	$(OBJDIR)/pstree.o \
	$(OBJDIR)/psutils.o \
	$(OBJDIR)/pswalk.o \
	$(OBJDIR)/psxface.o \
	$(OBJDIR)/rsaddr.o \
	$(OBJDIR)/rscalc.o \
	$(OBJDIR)/rscreate.o \
	$(OBJDIR)/rsdump.o \
	$(OBJDIR)/rsinfo.o \
	$(OBJDIR)/rsio.o \
	$(OBJDIR)/rsirq.o \
	$(OBJDIR)/rslist.o \
	$(OBJDIR)/rsmemory.o \
	$(OBJDIR)/rsmisc.o \
	$(OBJDIR)/rsserial.o \
	$(OBJDIR)/rsutils.o \
	$(OBJDIR)/rsxface.o \
	$(OBJDIR)/tbfadt.o \
	$(OBJDIR)/tbfind.o \
	$(OBJDIR)/tbinstal.o \
	$(OBJDIR)/tbutils.o \
	$(OBJDIR)/tbxface.o \
	$(OBJDIR)/tbxfroot.o \
	$(OBJDIR)/utaddress.o \
	$(OBJDIR)/utalloc.o \
	$(OBJDIR)/utcache.o \
	$(OBJDIR)/utcopy.o \
	$(OBJDIR)/utdebug.o \
	$(OBJDIR)/utdecode.o \
	$(OBJDIR)/utdelete.o \
	$(OBJDIR)/uteval.o \
	$(OBJDIR)/utglobal.o \
	$(OBJDIR)/utids.o \
	$(OBJDIR)/utinit.o \
	$(OBJDIR)/utlock.o \
	$(OBJDIR)/utmath.o \
	$(OBJDIR)/utmisc.o \
	$(OBJDIR)/utmutex.o \
	$(OBJDIR)/utobject.o \
	$(OBJDIR)/utresrc.o \
	$(OBJDIR)/utstate.o \
	$(OBJDIR)/uttrack.o \
	$(OBJDIR)/utosi.o \
	$(OBJDIR)/utxface.o \
	$(OBJDIR)/utxferror.o \
	$(OBJDIR)/utxfmutex.o

#
# Flags specific to acpiexec utility
#
CFLAGS+= \
    -DACPI_EXEC_APP \
    -I$(ACPIEXEC)
LDFLAGS += -lpthread -lrt

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
