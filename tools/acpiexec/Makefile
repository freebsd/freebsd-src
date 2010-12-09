#
# AcpiExec utility
#

PROG = acpiexec

ACPICA_SRC =    ../..
ACPICA_COMMON = $(ACPICA_SRC)/common
ACPICA_CORE =   $(ACPICA_SRC)
ACPICA_TOOLS =  $(ACPICA_SRC)/tools
ACPICA_OSL =    $(ACPICA_SRC)/os_specific/service_layers

NOMAN=	YES
CFLAGS+= -Wall -g -D_LINUX -DNDEBUG -D_CONSOLE -DACPI_EXEC_APP -Wstrict-prototypes -I$(ACPICA_SRC)/include 

COMPILE = $(CC) -c $(CFLAGS) -o$@ $?
LDFLAGS += -lpthread -lrt

OBJS = \
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
	dsfield.o \
	dsinit.o \
	dsmethod.o \
	dsmthdat.o \
	dsobject.o \
	dsopcode.o \
	dsutils.o \
	dswexec.o \
	dswload.o \
	dswscope.o \
	dswstate.o \
	evevent.o \
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
# Root rule
#
$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -lpthread -o $(PROG)
	$(COPYPROG)


#
# acpiexec source
#
aeexec.o :          $(ACPICA_TOOLS)/acpiexec/aeexec.c
	$(COMPILE)

aehandlers.o :      $(ACPICA_TOOLS)/acpiexec/aehandlers.c
	$(COMPILE)

aemain.o :          $(ACPICA_TOOLS)/acpiexec/aemain.c
	$(COMPILE)

aetables.o :        $(ACPICA_TOOLS)/acpiexec/aetables.c
	$(COMPILE)


#
# ACPICA core source - common
#
getopt.o :          $(ACPICA_COMMON)/getopt.c
	$(COMPILE)


#
# ACPICA core source
#
dbcmds.o :          $(ACPICA_CORE)/debugger/dbcmds.c
	$(COMPILE)

dbdisply.o :        $(ACPICA_CORE)/debugger/dbdisply.c
	$(COMPILE)

dbexec.o :          $(ACPICA_CORE)/debugger/dbexec.c
	$(COMPILE)

dbfileio.o :        $(ACPICA_CORE)/debugger/dbfileio.c
	$(COMPILE)

dbhistry.o :        $(ACPICA_CORE)/debugger/dbhistry.c
	$(COMPILE)

dbinput.o :         $(ACPICA_CORE)/debugger/dbinput.c
	$(COMPILE)

dbstats.o :         $(ACPICA_CORE)/debugger/dbstats.c
	$(COMPILE)

dbutils.o :         $(ACPICA_CORE)/debugger/dbutils.c
	$(COMPILE)

dbxface.o :         $(ACPICA_CORE)/debugger/dbxface.c
	$(COMPILE)

dmbuffer.o :        $(ACPICA_CORE)/disassembler/dmbuffer.c
	$(COMPILE)

dmnames.o :         $(ACPICA_CORE)/disassembler/dmnames.c
	$(COMPILE)

dmobject.o :        $(ACPICA_CORE)/disassembler/dmobject.c
	$(COMPILE)

dmopcode.o :        $(ACPICA_CORE)/disassembler/dmopcode.c
	$(COMPILE)

dmresrc.o :         $(ACPICA_CORE)/disassembler/dmresrc.c
	$(COMPILE)

dmresrcl.o :        $(ACPICA_CORE)/disassembler/dmresrcl.c
	$(COMPILE)

dmresrcs.o :        $(ACPICA_CORE)/disassembler/dmresrcs.c
	$(COMPILE)

dmutils.o :         $(ACPICA_CORE)/disassembler/dmutils.c
	$(COMPILE)

dmwalk.o :          $(ACPICA_CORE)/disassembler/dmwalk.c
	$(COMPILE)

dsfield.o :         $(ACPICA_CORE)/dispatcher/dsfield.c
	$(COMPILE)

dsinit.o :          $(ACPICA_CORE)/dispatcher/dsinit.c
	$(COMPILE)

dsmethod.o :        $(ACPICA_CORE)/dispatcher/dsmethod.c
	$(COMPILE)

dsmthdat.o :        $(ACPICA_CORE)/dispatcher/dsmthdat.c
	$(COMPILE)

dsobject.o :        $(ACPICA_CORE)/dispatcher/dsobject.c
	$(COMPILE)

dsopcode.o :        $(ACPICA_CORE)/dispatcher/dsopcode.c
	$(COMPILE)

dsutils.o :         $(ACPICA_CORE)/dispatcher/dsutils.c
	$(COMPILE)

dswexec.o :         $(ACPICA_CORE)/dispatcher/dswexec.c
	$(COMPILE)

dswload.o :         $(ACPICA_CORE)/dispatcher/dswload.c
	$(COMPILE)

dswscope.o :        $(ACPICA_CORE)/dispatcher/dswscope.c
	$(COMPILE)

dswstate.o :        $(ACPICA_CORE)/dispatcher/dswstate.c
	$(COMPILE)

evevent.o :         $(ACPICA_CORE)/events/evevent.c
	$(COMPILE)

evgpe.o :           $(ACPICA_CORE)/events/evgpe.c
	$(COMPILE)

evgpeblk.o :        $(ACPICA_CORE)/events/evgpeblk.c
	$(COMPILE)

evgpeinit.o :       $(ACPICA_CORE)/events/evgpeinit.c
	$(COMPILE)

evgpeutil.o :       $(ACPICA_CORE)/events/evgpeutil.c
	$(COMPILE)

evmisc.o :          $(ACPICA_CORE)/events/evmisc.c
	$(COMPILE)

evregion.o :        $(ACPICA_CORE)/events/evregion.c
	$(COMPILE)

evrgnini.o :        $(ACPICA_CORE)/events/evrgnini.c
	$(COMPILE)

evsci.o :           $(ACPICA_CORE)/events/evsci.c
	$(COMPILE)

evxface.o :         $(ACPICA_CORE)/events/evxface.c
	$(COMPILE)

evxfevnt.o :        $(ACPICA_CORE)/events/evxfevnt.c
	$(COMPILE)

evxfgpe.o :         $(ACPICA_CORE)/events/evxfgpe.c
	$(COMPILE)

evxfregn.o :        $(ACPICA_CORE)/events/evxfregn.c
	$(COMPILE)

exconfig.o :        $(ACPICA_CORE)/executer/exconfig.c
	$(COMPILE)

exconvrt.o :        $(ACPICA_CORE)/executer/exconvrt.c
	$(COMPILE)

excreate.o :        $(ACPICA_CORE)/executer/excreate.c
	$(COMPILE)

exdebug.o :         $(ACPICA_CORE)/executer/exdebug.c
	$(COMPILE)

exdump.o :          $(ACPICA_CORE)/executer/exdump.c
	$(COMPILE)

exfield.o :         $(ACPICA_CORE)/executer/exfield.c
	$(COMPILE)

exfldio.o :         $(ACPICA_CORE)/executer/exfldio.c
	$(COMPILE)

exmisc.o :          $(ACPICA_CORE)/executer/exmisc.c
	$(COMPILE)

exmutex.o :         $(ACPICA_CORE)/executer/exmutex.c
	$(COMPILE)

exnames.o :         $(ACPICA_CORE)/executer/exnames.c
	$(COMPILE)

exoparg1.o :        $(ACPICA_CORE)/executer/exoparg1.c
	$(COMPILE)

exoparg2.o :        $(ACPICA_CORE)/executer/exoparg2.c
	$(COMPILE)

exoparg3.o :        $(ACPICA_CORE)/executer/exoparg3.c
	$(COMPILE)

exoparg6.o :        $(ACPICA_CORE)/executer/exoparg6.c
	$(COMPILE)

exprep.o :          $(ACPICA_CORE)/executer/exprep.c
	$(COMPILE)

exregion.o :        $(ACPICA_CORE)/executer/exregion.c
	$(COMPILE)

exresnte.o :        $(ACPICA_CORE)/executer/exresnte.c
	$(COMPILE)

exresolv.o :        $(ACPICA_CORE)/executer/exresolv.c
	$(COMPILE)

exresop.o :         $(ACPICA_CORE)/executer/exresop.c
	$(COMPILE)

exstore.o :         $(ACPICA_CORE)/executer/exstore.c
	$(COMPILE)

exstoren.o :        $(ACPICA_CORE)/executer/exstoren.c
	$(COMPILE)

exstorob.o :        $(ACPICA_CORE)/executer/exstorob.c
	$(COMPILE)

exsystem.o :        $(ACPICA_CORE)/executer/exsystem.c
	$(COMPILE)

exutils.o :         $(ACPICA_CORE)/executer/exutils.c
	$(COMPILE)

hwacpi.o :          $(ACPICA_CORE)/hardware/hwacpi.c
	$(COMPILE)

hwgpe.o :           $(ACPICA_CORE)/hardware/hwgpe.c
	$(COMPILE)

hwpci.o :           $(ACPICA_CORE)/hardware/hwpci.c
	$(COMPILE)

hwregs.o :          $(ACPICA_CORE)/hardware/hwregs.c
	$(COMPILE)

hwsleep.o :         $(ACPICA_CORE)/hardware/hwsleep.c
	$(COMPILE)

hwvalid.o :         $(ACPICA_CORE)/hardware/hwvalid.c
	$(COMPILE)

hwxface.o :         $(ACPICA_CORE)/hardware/hwxface.c
	$(COMPILE)

nsaccess.o :        $(ACPICA_CORE)/namespace/nsaccess.c
	$(COMPILE)

nsalloc.o :         $(ACPICA_CORE)/namespace/nsalloc.c
	$(COMPILE)

nsdump.o :          $(ACPICA_CORE)/namespace/nsdump.c
	$(COMPILE)

nsdumpdv.o :        $(ACPICA_CORE)/namespace/nsdumpdv.c
	$(COMPILE)

nseval.o :          $(ACPICA_CORE)/namespace/nseval.c
	$(COMPILE)

nsinit.o :          $(ACPICA_CORE)/namespace/nsinit.c
	$(COMPILE)

nsload.o :          $(ACPICA_CORE)/namespace/nsload.c
	$(COMPILE)

nsnames.o :         $(ACPICA_CORE)/namespace/nsnames.c
	$(COMPILE)

nsobject.o :        $(ACPICA_CORE)/namespace/nsobject.c
	$(COMPILE)

nsparse.o :         $(ACPICA_CORE)/namespace/nsparse.c
	$(COMPILE)

nspredef.o :        $(ACPICA_CORE)/namespace/nspredef.c
	$(COMPILE)

nsrepair.o :        $(ACPICA_CORE)/namespace/nsrepair.c
	$(COMPILE)

nsrepair2.o :       $(ACPICA_CORE)/namespace/nsrepair2.c
	$(COMPILE)

nssearch.o :        $(ACPICA_CORE)/namespace/nssearch.c
	$(COMPILE)

nsutils.o :         $(ACPICA_CORE)/namespace/nsutils.c
	$(COMPILE)

nswalk.o :          $(ACPICA_CORE)/namespace/nswalk.c
	$(COMPILE)

nsxfeval.o :        $(ACPICA_CORE)/namespace/nsxfeval.c
	$(COMPILE)

nsxfname.o :        $(ACPICA_CORE)/namespace/nsxfname.c
	$(COMPILE)

nsxfobj.o :         $(ACPICA_CORE)/namespace/nsxfobj.c
	$(COMPILE)

psargs.o :          $(ACPICA_CORE)/parser/psargs.c
	$(COMPILE)

psloop.o :          $(ACPICA_CORE)/parser/psloop.c
	$(COMPILE)

psopcode.o :        $(ACPICA_CORE)/parser/psopcode.c
	$(COMPILE)

psparse.o :         $(ACPICA_CORE)/parser/psparse.c
	$(COMPILE)

psscope.o :         $(ACPICA_CORE)/parser/psscope.c
	$(COMPILE)

pstree.o :          $(ACPICA_CORE)/parser/pstree.c
	$(COMPILE)

psutils.o :         $(ACPICA_CORE)/parser/psutils.c
	$(COMPILE)

pswalk.o :          $(ACPICA_CORE)/parser/pswalk.c
	$(COMPILE)

psxface.o :         $(ACPICA_CORE)/parser/psxface.c
	$(COMPILE)

rsaddr.o :          $(ACPICA_CORE)/resources/rsaddr.c
	$(COMPILE)

rscalc.o :          $(ACPICA_CORE)/resources/rscalc.c
	$(COMPILE)

rscreate.o :        $(ACPICA_CORE)/resources/rscreate.c
	$(COMPILE)

rsdump.o :          $(ACPICA_CORE)/resources/rsdump.c
	$(COMPILE)

rsio.o :            $(ACPICA_CORE)/resources/rsio.c
	$(COMPILE)

rsinfo.o :          $(ACPICA_CORE)/resources/rsinfo.c
	$(COMPILE)

rsirq.o :           $(ACPICA_CORE)/resources/rsirq.c
	$(COMPILE)

rslist.o :          $(ACPICA_CORE)/resources/rslist.c
	$(COMPILE)

rsmemory.o :        $(ACPICA_CORE)/resources/rsmemory.c
	$(COMPILE)

rsmisc.o :          $(ACPICA_CORE)/resources/rsmisc.c
	$(COMPILE)

rsutils.o :         $(ACPICA_CORE)/resources/rsutils.c
	$(COMPILE)

rsxface.o :         $(ACPICA_CORE)/resources/rsxface.c
	$(COMPILE)

tbfadt.o :          $(ACPICA_CORE)/tables/tbfadt.c
	$(COMPILE)

tbfind.o :          $(ACPICA_CORE)/tables/tbfind.c
	$(COMPILE)

tbinstal.o :        $(ACPICA_CORE)/tables/tbinstal.c
	$(COMPILE)

tbutils.o :         $(ACPICA_CORE)/tables/tbutils.c
	$(COMPILE)

tbxface.o :         $(ACPICA_CORE)/tables/tbxface.c
	$(COMPILE)

tbxfroot.o :        $(ACPICA_CORE)/tables/tbxfroot.c
	$(COMPILE)

utalloc.o :         $(ACPICA_CORE)/utilities/utalloc.c
	$(COMPILE)

utcache.o :         $(ACPICA_CORE)/utilities/utcache.c
	$(COMPILE)

utcopy.o :          $(ACPICA_CORE)/utilities/utcopy.c
	$(COMPILE)

utdebug.o :         $(ACPICA_CORE)/utilities/utdebug.c
	$(COMPILE)

utdelete.o :        $(ACPICA_CORE)/utilities/utdelete.c
	$(COMPILE)

uteval.o :          $(ACPICA_CORE)/utilities/uteval.c
	$(COMPILE)

utglobal.o :        $(ACPICA_CORE)/utilities/utglobal.c
	$(COMPILE)

utids.o :           $(ACPICA_CORE)/utilities/utids.c
	$(COMPILE)

utinit.o :          $(ACPICA_CORE)/utilities/utinit.c
	$(COMPILE)

utlock.o :          $(ACPICA_CORE)/utilities/utlock.c
	$(COMPILE)

utmath.o :          $(ACPICA_CORE)/utilities/utmath.c
	$(COMPILE)

utmisc.o :          $(ACPICA_CORE)/utilities/utmisc.c
	$(COMPILE)

utmutex.o :         $(ACPICA_CORE)/utilities/utmutex.c
	$(COMPILE)

utobject.o :        $(ACPICA_CORE)/utilities/utobject.c
	$(COMPILE)

utresrc.o :         $(ACPICA_CORE)/utilities/utresrc.c
	$(COMPILE)

utstate.o :         $(ACPICA_CORE)/utilities/utstate.c
	$(COMPILE)

uttrack.o :         $(ACPICA_CORE)/utilities/uttrack.c
	$(COMPILE)

utosi.o :           $(ACPICA_CORE)/utilities/utosi.c
	$(COMPILE)

utxferror.o :       $(ACPICA_CORE)/utilities/utxferror.c
	$(COMPILE)

utxface.o :         $(ACPICA_CORE)/utilities/utxface.c
	$(COMPILE)


#
# Unix OS services layer (OSL)
#
osunixxf.o :        $(ACPICA_OSL)/osunixxf.c
	$(COMPILE)


clean : 
	rm -f $(PROG) $(PROG).exe $(OBJS)
