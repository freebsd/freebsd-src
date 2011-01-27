# $FreeBSD$

.if !defined(FORCE_BUILD)
.error "The ACPI module is deprecated, set FORCE_BUILD to force it"
.endif

.if ${MACHINE_CPUARCH} == "ia64"
.error "ACPI can only be compiled into the kernel on the ia64 platform"
.endif

.if ${MACHINE} != "amd64" && ${MACHINE} != "i386"
.error "The ACPI module is only for amd64 and i386"
.endif

.PATH:	${.CURDIR}/../../../contrib/dev/acpica/debugger \
	${.CURDIR}/../../../contrib/dev/acpica/disassembler \
	${.CURDIR}/../../../contrib/dev/acpica/dispatcher \
	${.CURDIR}/../../../contrib/dev/acpica/events \
	${.CURDIR}/../../../contrib/dev/acpica/executer \
	${.CURDIR}/../../../contrib/dev/acpica/hardware \
	${.CURDIR}/../../../contrib/dev/acpica/namespace \
	${.CURDIR}/../../../contrib/dev/acpica/parser \
	${.CURDIR}/../../../contrib/dev/acpica/resources \
	${.CURDIR}/../../../contrib/dev/acpica/tables \
	${.CURDIR}/../../../contrib/dev/acpica/utilities \
	${.CURDIR}/../../../pci \
	${.CURDIR}/../../../dev/acpica \
	${.CURDIR}/../../../dev/acpica/Osd \
	${.CURDIR}/../../../${MACHINE_CPUARCH}/acpica \
	${.CURDIR}/../../../x86/acpica

KMOD=	acpi

# ACPI CA sources
SRCS+=	dbcmds.c dbdisply.c dbexec.c dbfileio.c dbhistry.c dbinput.c dbstats.c
SRCS+=	dbutils.c dbxface.c
SRCS+=	dmbuffer.c dmnames.c dmopcode.c dmobject.c dmresrc.c dmresrcl.c
SRCS+=	dmresrcs.c dmutils.c dmwalk.c
SRCS+=	dsfield.c dsinit.c dsmethod.c dsmthdat.c dsobject.c dsopcode.c
SRCS+=	dsutils.c dswexec.c dswload.c dswscope.c dswstate.c
SRCS+=	evevent.c evgpe.c evgpeblk.c evgpeinit.c evgpeutil.c evmisc.c
SRCS+=	evregion.c evrgnini.c evsci.c evxface.c evxfevnt.c evxfgpe.c evxfregn.c
SRCS+=	exconfig.c exconvrt.c excreate.c exdebug.c exdump.c exfield.c
SRCS+=	exfldio.c exmisc.c exmutex.c exnames.c exoparg1.c exoparg2.c
SRCS+=	exoparg3.c exoparg6.c exprep.c exregion.c exresnte.c exresolv.c
SRCS+=	exresop.c exstore.c exstoren.c exstorob.c exsystem.c exutils.c
SRCS+=	hwacpi.c hwgpe.c hwpci.c hwregs.c hwsleep.c hwtimer.c hwvalid.c
SRCS+=	hwxface.c
SRCS+=	nsaccess.c nsalloc.c nsdump.c nseval.c nsinit.c nsload.c nsnames.c
SRCS+=	nsobject.c nsparse.c nspredef.c nsrepair.c nsrepair2.c nssearch.c
SRCS+=	nsutils.c nswalk.c nsxfeval.c nsxfname.c nsxfobj.c
SRCS+=	psargs.c psloop.c psopcode.c psparse.c psscope.c pstree.c psutils.c
SRCS+=	pswalk.c psxface.c
SRCS+=	rsaddr.c rscalc.c rscreate.c rsdump.c rsinfo.c rsio.c rsirq.c rslist.c
SRCS+=	rsmemory.c rsmisc.c rsutils.c rsxface.c
SRCS+=	tbfadt.c tbfind.c tbinstal.c tbutils.c tbxface.c tbxfroot.c
SRCS+=	utalloc.c utcache.c utcopy.c utdebug.c utdelete.c uteval.c utglobal.c
SRCS+=	utids.c utinit.c utlock.c utmath.c utmisc.c utmutex.c utobject.c
SRCS+=	utosi.c utresrc.c utstate.c utxface.c utxferror.c

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
SRCS+=	opt_kstack_pages.h opt_nfs.h opt_apic.h opt_compat.h opt_hwpmc_hooks.h

.if !defined(KERNBUILDDIR)
.if KTR
CFLAGS+=-DKTR
.endif
.if ACPI_MAX_TASKS
CFLAGS+=-DACPI_MAX_TASKS=${ACPI_MAX_TASKS}
.endif
.if ACPI_MAX_THREADS
CFLAGS+=-DACPI_MAX_THREADS=${ACPI_MAX_THREADS}
.endif
.if ACPI_DEBUG
CFLAGS+=-DACPI_DEBUG
opt_ddb.h: Makefile
	echo "#define DDB 1" > ${.TARGET}
.else
opt_ddb.h: Makefile
	echo -n > ${.TARGET}
.endif
.endif

# Machine-specific code such as sleep/wakeup
SRCS+=	acpi_apm.c acpi_machdep.c acpi_wakecode.h acpi_wakedata.h acpi_wakeup.c
SRCS+=	assym.s madt.c
CLEANFILES+=acpi_wakecode.bin acpi_wakecode.h acpi_wakecode.o acpi_wakedata.h

SRCS+=	opt_global.h

ASM_CFLAGS=	-x assembler-with-cpp -DLOCORE ${CFLAGS}
NORMAL_S=	${CC} -c ${ASM_CFLAGS} ${WERROR} ${.IMPSRC}
NM?=		nm

.if ${MACHINE_CPUARCH} == "amd64"
.if !defined(KERNBUILDDIR)
CFLAGS+=-DSMP
.endif
SRCS+=	acpi_switch.S
acpi_switch.o:	acpi_switch.S
	${NORMAL_S}
.endif
acpi_wakecode.o: acpi_wakecode.S assym.s
	${NORMAL_S}
acpi_wakecode.bin: acpi_wakecode.o
	objcopy -S -O binary acpi_wakecode.o ${.TARGET}
acpi_wakecode.h: acpi_wakecode.bin
	file2c -sx 'static char wakecode[] = {' '};' < acpi_wakecode.bin > \
	    ${.TARGET}
acpi_wakedata.h: acpi_wakecode.o
	${NM} -n --defined-only acpi_wakecode.o | \
	    while read offset dummy what; do \
	    echo "#define	$${what}	0x$${offset}"; \
	    done > ${.TARGET}

.include <bsd.kmod.mk>
