# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/dev/acpica \
       ${.CURDIR}/../../pci ${.CURDIR}/../../dev/acpica \
       ${.CURDIR}/../../dev/acpica/Osd

KMOD=	acpica

# ACPI CA sources
SRCS+=	dsfield.c dsmethod.c dsmthdat.c dsobject.c dsopcode.c dsutils.c
SRCS+=	dswexec.c dswload.c dswscope.c dswstate.c evevent.c evmisc.c evregion.c
SRCS+=	evrgnini.c evsci.c evxface.c evxfevnt.c evxfregn.c exconfig.c exconvrt.c
SRCS+=	excreate.c exdump.c exdyadic.c exfield.c exfldio.c exmisc.c exmonad.c
SRCS+=	exmutex.c exnames.c exprep.c exregion.c exresnte.c exresolv.c exresop.c
SRCS+=	exstore.c exstoren.c exstorob.c exsystem.c exutils.c exxface.c hwacpi.c
SRCS+=	hwgpe.c hwregs.c hwsleep.c hwtimer.c nsaccess.c nsalloc.c nsdump.c
SRCS+=	nseval.c nsinit.c nsload.c nsnames.c nsobject.c nssearch.c nsutils.c
SRCS+=	nswalk.c nsxfname.c nsxfobj.c psargs.c psfind.c psopcode.c psparse.c
SRCS+=	psscope.c pstree.c psutils.c pswalk.c psxface.c rsaddr.c rscalc.c
SRCS+=  rscreate.c rsdump.c rsio.c rsirq.c rslist.c rsmemory.c rsmisc.c
SRCS+=  rsutils.c rsxface.c tbconvrt.c tbget.c tbinstal.c tbutils.c tbxface.c
SRCS+=	tbxfroot.c utalloc.c utclib.c utcopy.c utdebug.c utdelete.c uteval.c
SRCS+=	utglobal.c utinit.c utmisc.c utobject.c utxface.c

# ACPI CA debug support
#SRCS=	dbcmds.c dbdisasm.c dbdisply.c dbexec.c dbfileio.c dbhistry.c dbinput.c
#SRCS+=	dbstats.c dbutils.c dbxface.c 

# OSD layer
SRCS+=	acpi.c acpi_acad.c acpi_battery.c acpi_button.c acpi_cmbat.c acpi_cpu.c
SRCS+=	acpi_ec.c acpi_isa.c acpi_lid.c acpi_pcib.c acpi_powerprofile.c
SRCS+=	acpi_powerres.c acpi_resource.c acpi_thermal.c acpi_timer.c
SRCS+=  OsdDebug.c 
SRCS+=	OsdHardware.c OsdInterrupt.c OsdMemory.c OsdSchedule.c
SRCS+=	OsdStream.c OsdSynch.c OsdEnvironment.c 
SRCS+=	opt_acpi.h opt_ddb.h
SRCS+=	device_if.h bus_if.h pci_if.h pcib_if.h isa_if.h

CFLAGS+=   -I${.CURDIR}/../../contrib/dev/acpica

opt_ddb.h: Makefile
	echo '#define DDB 1'  >opt_ddb.h

.include <bsd.kmod.mk>
