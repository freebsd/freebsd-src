# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/dev/acpica \
       ${.CURDIR}/../../pci \
       ${.CURDIR}/../../dev/acpica \
       ${.CURDIR}/../../dev/acpica/Osd \
       ${.CURDIR}/../../${MACHINE_ARCH}/acpica

KMOD=	acpi

# ACPI CA sources
CFLAGS+=   -I${.CURDIR}/../../contrib/dev/acpica
SRCS+=	dsfield.c dsmethod.c dsmthdat.c dsobject.c dsopcode.c
SRCS+=	dsutils.c dswexec.c dswload.c dswscope.c dswstate.c
SRCS+=	evevent.c evmisc.c evregion.c evrgnini.c evsci.c
SRCS+=	evxface.c evxfevnt.c evxfregn.c
SRCS+=	exconfig.c exconvrt.c excreate.c exdump.c exfield.c
SRCS+=	exfldio.c exmisc.c exmutex.c exnames.c exoparg1.c
SRCS+=	exoparg2.c exoparg3.c exoparg6.c exprep.c exregion.c
SRCS+=	exresnte.c exresolv.c exresop.c exstore.c exstoren.c
SRCS+=	exstorob.c exsystem.c exutils.c
SRCS+=	hwacpi.c hwgpe.c hwregs.c hwsleep.c hwtimer.c
SRCS+=	nsaccess.c nsalloc.c nsdump.c nseval.c nsinit.c
SRCS+=	nsload.c nsnames.c nsobject.c nssearch.c nsutils.c
SRCS+=	nswalk.c nsxfeval.c nsxfname.c nsxfobj.c
SRCS+=	psargs.c psfind.c psopcode.c psparse.c psscope.c
SRCS+=	pstree.c psutils.c pswalk.c psxface.c
SRCS+=	rsaddr.c rscalc.c rscreate.c rsdump.c rsio.c
SRCS+=	rsirq.c rslist.c rsmemory.c rsmisc.c rsutils.c
SRCS+=	rsxface.c
SRCS+=	tbconvrt.c tbget.c tbgetall.c tbinstal.c tbrsdt.c
SRCS+=	tbutils.c tbxface.c tbxfroot.c
SRCS+=	utalloc.c utclib.c utcopy.c utdebug.c utdelete.c
SRCS+=	uteval.c utglobal.c utinit.c utmath.c utmisc.c
SRCS+=	utobject.c utxface.c

# OSD layer
SRCS+=	acpi.c acpi_acad.c acpi_battery.c acpi_button.c acpi_cmbat.c acpi_cpu.c
SRCS+=	acpi_ec.c acpi_lid.c acpi_pci.c acpi_pcib.c acpi_pcib_acpi.c
SRCS+=	acpi_pcib_pci.c acpi_powerres.c acpi_resource.c acpi_thermal.c
SRCS+=	acpi_timer.c acpica_support.c
SRCS+=  OsdDebug.c 
SRCS+=	OsdHardware.c OsdInterrupt.c OsdMemory.c OsdSchedule.c
SRCS+=	OsdStream.c OsdSynch.c OsdTable.c OsdEnvironment.c 
SRCS+=	opt_acpi.h opt_bus.h opt_ddb.h
SRCS+=	device_if.h bus_if.h pci_if.h pcib_if.h isa_if.h
.if ACPI_NO_SEMAPHORES
CFLAGS+=-DACPI_NO_SEMAPHORES
.endif
.if ACPI_MAX_THREADS
CFLAGS+=-DACPI_MAX_THREADS=${ACPI_MAX_THREADS}
.endif

# Debugging support
.if ACPI_DEBUG
CFLAGS+=-DACPI_DEBUG
SRCS+=	dbcmds.c dbdisply.c dbexec.c dbfileio.c dbhistry.c
SRCS+=	dbinput.c dbstats.c dbutils.c dbxface.c
SRCS+=	dmbuffer.c dmnames.c dmopcode.c dmresrc.c dmresrcl.c
SRCS+=	dmresrcs.c dmutils.c dmwalk.c
opt_ddb.h: Makefile
	echo '#define DDB 1' > ${.TARGET}
.else
opt_ddb.h: Makefile
	echo -n > ${.TARGET}
.endif

# Machine-specific code such as sleep/wakeup
SRCS+=	acpi_machdep.c acpi_wakecode.h acpi_wakeup.c
CLEANFILES+=	acpi_wakecode.h acpi_wakecode.o acpi_wakecode.bin

acpi_wakecode.h: acpi_wakecode.S
	${MAKE} -f ${.CURDIR}/../../${MACHINE_ARCH}/acpica/Makefile \
		MAKESRCPATH=${.CURDIR}/../../${MACHINE_ARCH}/acpica

.include <bsd.kmod.mk>
