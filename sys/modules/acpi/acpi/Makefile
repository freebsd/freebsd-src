# $FreeBSD$

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "ia64"
.error "ACPI can only be compiled into the kernel on the amd64 and ia64 platforms"
.endif

.if ${MACHINE} != "i386"
.error "The ACPI module is only for i386"
.endif

.PATH: ${.CURDIR}/../../../contrib/dev/acpica \
       ${.CURDIR}/../../../pci \
       ${.CURDIR}/../../../dev/acpica \
       ${.CURDIR}/../../../dev/acpica/Osd \
       ${.CURDIR}/../../../${MACHINE_ARCH}/acpica

KMOD=	acpi

# ACPI CA sources
CFLAGS+=   -I${.CURDIR}/../../../contrib/dev/acpica
SRCS+=	dsfield.c dsinit.c dsmethod.c dsmthdat.c dsobject.c dsopcode.c
SRCS+=	dsutils.c dswexec.c dswload.c dswscope.c dswstate.c
SRCS+=	evevent.c evgpe.c evgpeblk.c evmisc.c evregion.c evrgnini.c evsci.c
SRCS+=	evxface.c evxfevnt.c evxfregn.c
SRCS+=	exconfig.c exconvrt.c excreate.c exdump.c exfield.c exfldio.c exmisc.c
SRCS+=	exmutex.c exnames.c exoparg1.c exoparg2.c exoparg3.c exoparg6.c
SRCS+=	exprep.c exregion.c exresnte.c exresolv.c exresop.c exstore.c
SRCS+=	exstoren.c exstorob.c exsystem.c exutils.c
SRCS+=	hwacpi.c hwgpe.c hwregs.c hwsleep.c hwtimer.c
SRCS+=	nsaccess.c nsalloc.c nsdump.c nseval.c nsinit.c nsload.c nsnames.c
SRCS+=	nsobject.c nsparse.c nssearch.c nsutils.c nswalk.c nsxfeval.c
SRCS+=	nsxfname.c nsxfobj.c
SRCS+=	psargs.c psloop.c psopcode.c psparse.c psscope.c pstree.c psutils.c
SRCS+=	pswalk.c psxface.c
SRCS+=	rsaddr.c rscalc.c rscreate.c rsdump.c rsinfo.c rsio.c rsirq.c rslist.c
SRCS+=	rsmemory.c rsmisc.c rsutils.c rsxface.c
SRCS+=	tbfadt.c tbfind.c tbinstal.c tbutils.c tbxface.c tbxfroot.c
SRCS+=	utalloc.c utcache.c utclib.c utcopy.c utdebug.c utdelete.c uteval.c
SRCS+=	utglobal.c utinit.c utmath.c utmisc.c utmutex.c utobject.c utresrc.c
SRCS+=	utstate.c utxface.c

# OSPM layer and core hardware drivers
SRCS+=	acpi.c acpi_button.c acpi_isab.c acpi_package.c acpi_pci.c acpi_pcib.c
SRCS+=	acpi_pcib_acpi.c acpi_pcib_pci.c acpi_powerres.c acpi_quirk.c
SRCS+=	acpi_resource.c acpi_timer.c acpi_pci_link.c acpi_thermal.c

# ACPI hardware drivers, mostly used for mobile systems.
SRCS+=	acpi_acad.c acpi_battery.c acpi_cmbat.c acpi_cpu.c acpi_ec.c
SRCS+=	acpi_hpet.c acpi_lid.c acpi_perf.c acpi_smbat.c acpi_throttle.c

# OSD layer
SRCS+=	OsdDebug.c
SRCS+=	OsdHardware.c OsdInterrupt.c OsdMemory.c OsdSchedule.c OsdStream.c
SRCS+=	OsdSynch.c OsdTable.c OsdEnvironment.c
SRCS+=	opt_acpi.h opt_bus.h opt_ddb.h acpi_if.h acpi_quirks.h bus_if.h
SRCS+=	cpufreq_if.h device_if.h isa_if.h pci_if.h pcib_if.h

# XXX ACPI should not depend on the following headers but this is currently
# needed for the build of assym.s.
# This obviously needs a better and more structural fix.
SRCS+=	opt_kstack_pages.h opt_nfs.h opt_apic.h opt_compat.h

# Debugging support
DBSRC=	dbcmds.c dbdisply.c dbexec.c dbfileio.c dbhistry.c dbinput.c dbstats.c
DBSRC+=	dbutils.c dbxface.c
DBSRC+=	dmbuffer.c dmnames.c dmopcode.c dmobject.c dmresrc.c dmresrcl.c
DBSRC+=	dmresrcs.c dmutils.c dmwalk.c

.if !defined(KERNBUILDDIR)
.if ACPI_MAX_THREADS
CFLAGS+=-DACPI_MAX_THREADS=${ACPI_MAX_THREADS}
.endif
.if ACPI_NO_SEMAPHORES
CFLAGS+=-DACPI_NO_SEMAPHORES
.endif
.if ACPI_DEBUG
CFLAGS+=-DACPI_DEBUG
SRCS+=	${DBSRC}
opt_ddb.h: Makefile
	echo "#define DDB 1" > ${.TARGET}
.else
opt_ddb.h: Makefile
	echo -n > ${.TARGET}
.endif
.endif

# Machine-specific code such as sleep/wakeup
SRCS+=	acpi_machdep.c acpi_wakecode.h acpi_wakeup.c
.if ${MACHINE} == "i386"
SRCS+=	madt.c assym.s
.endif
CLEANFILES+=	acpi_wakecode.h acpi_wakecode.o acpi_wakecode.bin ${DBSRC:.c=.o}

acpi_wakecode.h: acpi_wakecode.S assym.s
	${MAKE} -f ${.CURDIR}/../../../${MACHINE_ARCH}/acpica/Makefile \
		MAKESRCPATH=${.CURDIR}/../../../${MACHINE_ARCH}/acpica

.include <bsd.kmod.mk>
