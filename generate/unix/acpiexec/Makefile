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
PROG = acpiexec

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
	aeexec.o \
	aehandlers.o \
	aemain.o \
	aetables.o \
	dbcmds.o \
	dbdisply.o \
	dbexec.o \
	dbfileio.o \
	dbhistry.o \
	dbinput.o \
	dbmethod.o \
	dbnames.o \
	dbstats.o \
	dbutils.o \
	dbxface.o \
	dmbuffer.o \
	dmnames.o \
	dmobject.o \
	dmopcode.o \
	dmresrc.o \
	dmresrcl.o \
	dmresrcs.o \
	dmutils.o \
	dmwalk.o \
	dsargs.o \
	dscontrol.o \
	dsfield.o \
	dsinit.o \
	dsmethod.o \
	dsmthdat.o \
	dsobject.o \
	dsopcode.o \
	dsutils.o \
	dswexec.o \
	dswload.o \
	dswload2.o \
	dswscope.o \
	dswstate.o \
	evevent.o \
	evglock.o \
	evgpe.o \
	evgpeblk.o \
	evgpeinit.o \
	evgpeutil.o \
	evmisc.o \
	evregion.o \
	evrgnini.o \
	evsci.o \
	evxface.o \
	evxfevnt.o \
	evxfgpe.o \
	evxfregn.o \
	exconfig.o \
	exconvrt.o \
	excreate.o \
	exdebug.o \
	exdump.o \
	exfield.o \
	exfldio.o \
	exmisc.o \
	exmutex.o \
	exnames.o \
	exoparg1.o \
	exoparg2.o \
	exoparg3.o \
	exoparg6.o \
	exprep.o \
	exregion.o \
	exresnte.o \
	exresolv.o \
	exresop.o \
	exstore.o \
	exstoren.o \
	exstorob.o \
	exsystem.o \
	exutils.o \
	getopt.o \
	hwacpi.o \
	hwgpe.o \
	hwpci.o \
	hwregs.o \
	hwsleep.o \
	hwvalid.o \
	hwxface.o \
	nsaccess.o \
	nsalloc.o \
	nsdump.o \
	nsdumpdv.o \
	nseval.o \
	nsinit.o \
	nsload.o \
	nsnames.o \
	nsobject.o \
	nsparse.o \
	nspredef.o \
	nsrepair.o \
	nsrepair2.o \
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
	rsaddr.o \
	rscalc.o \
	rscreate.o \
	rsdump.o \
	rsinfo.o \
	rsio.o \
	rsirq.o \
	rslist.o \
	rsmemory.o \
	rsmisc.o \
	rsutils.o \
	rsxface.o \
	tbfadt.o \
	tbfind.o \
	tbinstal.o \
	tbutils.o \
	tbxface.o \
	tbxfroot.o \
	utalloc.o \
	utcache.o \
	utcopy.o \
	utdebug.o \
	utdecode.o \
	utdelete.o \
	uteval.o \
	utglobal.o \
	utids.o \
	utinit.o \
	utlock.o \
	utmath.o \
	utmisc.o \
	utmutex.o \
	utobject.o \
	utresrc.o \
	utstate.o \
	uttrack.o \
	utosi.o \
	utxferror.o \
	utxface.o

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

%.o : %.c $(HEADERS) $(ACPICA_HEADERS)
	$(COMPILE)

clean :
	rm -f $(PROG) $(PROG).exe $(OBJECTS)

install :
	$(INSTALLPROG)
