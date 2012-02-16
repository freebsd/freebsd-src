#
# acpiexec: ACPI execution simulator. Runs ACPICA code in user
# space. Loads ACPI tables, displays the namespace, and allows
# execution of control methods.
#
# NOTE: This makefile is intended to be used within the native
# ACPICA source tree.
#

#
# Configuration
# Notes:
#   gcc should be version 4 or greater, otherwise some of the options
#       used will not be recognized.
#   Global optimization flags (such as -O2, -Os) are not used, since
#       they cause issues on some compilers.
#   The _GNU_SOURCE symbol is required for many hosts.
#
PROG = acpiexec

HOST =          _LINUX
NOMAN =         YES
COMPILE =       $(CC) -c $(CFLAGS) $(CWARNINGFLAGS) -o$@ $<
LDFLAGS +=      -lpthread -lrt

ACPICA_SRC =            ../../../source
ACPICA_COMMON =         $(ACPICA_SRC)/common
ACPICA_TOOLS =          $(ACPICA_SRC)/tools
ACPICA_OSL =            $(ACPICA_SRC)/os_specific/service_layers
ACPICA_CORE =           $(ACPICA_SRC)/components
ACPICA_INCLUDE =        $(ACPICA_SRC)/include
ACPICA_DEBUGGER =       $(ACPICA_CORE)/debugger
ACPICA_DISASSEMBLER =   $(ACPICA_CORE)/disassembler
ACPICA_DISPATCHER =     $(ACPICA_CORE)/dispatcher
ACPICA_EVENTS =         $(ACPICA_CORE)/events
ACPICA_EXECUTER =       $(ACPICA_CORE)/executer
ACPICA_HARDWARE =       $(ACPICA_CORE)/hardware
ACPICA_NAMESPACE =      $(ACPICA_CORE)/namespace
ACPICA_PARSER =         $(ACPICA_CORE)/parser
ACPICA_RESOURCES =      $(ACPICA_CORE)/resources
ACPICA_TABLES =         $(ACPICA_CORE)/tables
ACPICA_UTILITIES =      $(ACPICA_CORE)/utilities
ACPIEXEC =              $(ACPICA_TOOLS)/acpiexec
INSTALLDIR =            /usr/bin
INSTALLPROG =           cp --remove-destination $(PROG) $(INSTALLDIR)

ACPICA_HEADERS = \
    $(wildcard $(ACPICA_INCLUDE)/*.h) \
    $(wildcard $(ACPICA_INCLUDE)/platform/*.h)

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
	dmresrcl2.o \
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
	hwesleep.o \
	hwgpe.o \
	hwpci.o \
	hwregs.o \
	hwsleep.o \
	hwvalid.o \
	hwxface.o \
	hwxfsleep.o \
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
	rsserial.o \
	rsmisc.o \
	rsutils.o \
	rsxface.o \
	tbfadt.o \
	tbfind.o \
	tbinstal.o \
	tbutils.o \
	tbxface.o \
	tbxfroot.o \
	utaddress.o \
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
	utxface.o \
	utxfmutex.o

CFLAGS+= \
    -D$(HOST) \
    -D_GNU_SOURCE \
    -DACPI_EXEC_APP \
    -I$(ACPICA_INCLUDE)

CWARNINGFLAGS = \
    -ansi \
    -Wall \
    -Wbad-function-cast \
    -Wdeclaration-after-statement \
    -Werror \
    -Wformat=2 \
    -Wmissing-declarations \
    -Wmissing-prototypes \
    -Wstrict-aliasing=0 \
    -Wstrict-prototypes \
    -Wswitch-default \
    -Wpointer-arith \
    -Wundef

#
# gcc 4+ flags
#
CWARNINGFLAGS += \
    -Waddress \
    -Waggregate-return \
    -Wchar-subscripts \
    -Wempty-body \
    -Wlogical-op \
    -Wmissing-declarations \
    -Wmissing-field-initializers \
    -Wmissing-parameter-type \
    -Wnested-externs \
    -Wold-style-declaration \
    -Wold-style-definition \
    -Wredundant-decls \
    -Wtype-limits

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
