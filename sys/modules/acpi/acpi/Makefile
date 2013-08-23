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

.PATH:	${.CURDIR}/../../../contrib/dev/acpica/components/debugger \
	${.CURDIR}/../../../contrib/dev/acpica/components/disassembler \
	${.CURDIR}/../../../contrib/dev/acpica/components/dispatcher \
	${.CURDIR}/../../../contrib/dev/acpica/components/events \
	${.CURDIR}/../../../contrib/dev/acpica/components/executer \
	${.CURDIR}/../../../contrib/dev/acpica/components/hardware \
	${.CURDIR}/../../../contrib/dev/acpica/components/namespace \
	${.CURDIR}/../../../contrib/dev/acpica/components/parser \
	${.CURDIR}/../../../contrib/dev/acpica/components/resources \
	${.CURDIR}/../../../contrib/dev/acpica/components/tables \
	${.CURDIR}/../../../contrib/dev/acpica/components/utilities \
	${.CURDIR}/../../../pci \
	${.CURDIR}/../../../dev/acpica \
	${.CURDIR}/../../../dev/acpica/Osd \
	${.CURDIR}/../../../${MACHINE_CPUARCH}/acpica \
	${.CURDIR}/../../../x86/acpica

KMOD=	acpi

# ACPI CA sources
SRCS=	dbcmds.c dbconvert.c dbdisply.c dbexec.c dbfileio.c dbhistry.c
SRCS+=	dbinput.c dbmethod.c dbnames.c dbstats.c dbutils.c dbxface.c
SRCS+=	dmbuffer.c dmdeferred.c dmnames.c dmopcode.c dmobject.c dmresrc.c
SRCS+=	dmresrcl.c dmresrcl2.c dmresrcs.c dmutils.c dmwalk.c
SRCS+=	dsargs.c dscontrol.c dsfield.c dsinit.c dsmethod.c dsmthdat.c
SRCS+=	dsobject.c dsopcode.c dsutils.c dswexec.c dswload.c dswload2.c
SRCS+=	dswscope.c dswstate.c
SRCS+=	evevent.c evglock.c evgpe.c evgpeblk.c evgpeinit.c evgpeutil.c
SRCS+=	evhandler.c evmisc.c evregion.c evrgnini.c evsci.c evxface.c
SRCS+=	evxfevnt.c evxfgpe.c evxfregn.c
SRCS+=	exconfig.c exconvrt.c excreate.c exdebug.c exdump.c exfield.c
SRCS+=	exfldio.c exmisc.c exmutex.c exnames.c exoparg1.c exoparg2.c
SRCS+=	exoparg3.c exoparg6.c exprep.c exregion.c exresnte.c exresolv.c
SRCS+=	exresop.c exstore.c exstoren.c exstorob.c exsystem.c exutils.c
SRCS+=	hwacpi.c hwesleep.c hwgpe.c hwpci.c hwregs.c hwsleep.c hwtimer.c
SRCS+=	hwvalid.c hwxface.c hwxfsleep.c
SRCS+=	nsaccess.c nsalloc.c nsarguments.c nsconvert.c nsdump.c nseval.c
SRCS+=	nsinit.c nsload.c nsnames.c nsobject.c nsparse.c nspredef.c nsprepkg.c
SRCS+=	nsrepair.c nsrepair2.c nssearch.c nsutils.c nswalk.c nsxfeval.c
SRCS+=	nsxfname.c nsxfobj.c
SRCS+=	psargs.c psloop.c psobject.c psopcode.c psopinfo.c psparse.c
SRCS+=	psscope.c pstree.c psutils.c pswalk.c psxface.c
SRCS+=	rsaddr.c rscalc.c rscreate.c rsdump.c rsdumpinfo.c rsinfo.c rsio.c
SRCS+=	rsirq.c rslist.c rsmemory.c rsmisc.c rsserial.c rsutils.c rsxface.c
SRCS+=	tbfadt.c tbfind.c tbinstal.c tbprint.c tbutils.c tbxface.c tbxfload.c
SRCS+=	tbxfroot.c
SRCS+=	utaddress.c utalloc.c utbuffer.c utcache.c utcopy.c utdebug.c
SRCS+=	utdecode.c utdelete.c uterror.c uteval.c utexcep.c utglobal.c utids.c
SRCS+=	utinit.c utlock.c utmath.c utmisc.c utmutex.c utobject.c utosi.c
SRCS+=	utownerid.c utpredef.c utresrc.c utstate.c utstring.c utxface.c
SRCS+=	utxferror.c utxfinit.c
#SRCS+=	utxfmutex.c

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
.if SMP
CFLAGS+=-DSMP
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

.include <bsd.kmod.mk>

acpi_wakecode.o: acpi_wakecode.S assym.s
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} ${CLANG_NO_IAS} \
	    ${WERROR} ${.IMPSRC}
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
