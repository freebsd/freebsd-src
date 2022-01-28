# $FreeBSD$

LIB=	pmc

SRCS=	libpmc.c pmclog.c libpmc_pmu_util.c libpmc_json.cc
INCS=	pmc.h pmclog.h pmcformat.h

CFLAGS+= -I${SRCTOP}/${RELDIR:H}/libpmcstat
LDADD+=	-lc++

.if ${MACHINE_CPUARCH} == "aarch64"
EVENT_ARCH="arm64"
.elif ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
EVENT_ARCH="x86"
.elif ${MACHINE_CPUARCH} == "powerpc" && ${MACHINE_ARCH:Mpowerpc64*} != ""
EVENT_ARCH="powerpc"
.else
# This will generate an empty events table
EVENT_ARCH="none"
.endif

.if ${MK_DIRDEPS_BUILD} == "yes"
# avoid circular dependency
GENDIRDEPS_FILTER+= N${RELDIR:H}/libpmcstat
JEVENTS?= ${HOST_OBJTOP}/${RELDIR}/pmu-events/jevents
.else
JEVENTS= ${BTOOLSPATH:U.}/pmu-events/jevents

# This file is built in a subdirectory so never try to rebuild it here.
${JEVENTS}: .PHONY
.if make(*clean*)
SUBDIR+= pmu-events
.endif
.endif

libpmc_events.c: ${JEVENTS} .META
	${JEVENTS} ${EVENT_ARCH} ${.CURDIR}/pmu-events/arch ${.TARGET}.tmp
	if [ ! -e ${.TARGET} ] || ! cmp -s ${.TARGET} ${.TARGET}.tmp; then \
		mv -f ${.TARGET}.tmp ${.TARGET}; \
	fi
CLEANFILES+=	libpmc_events.c libpmc_events.c.tmp
SRCS+=	libpmc_events.c

WARNS?=	3

CFLAGS+= -I${.CURDIR}
CWARNFLAGS.gcc+= -Wno-shadow -Wno-cast-align

# Silence warnings about usage of deprecated std::auto_ptr
CXXWARNFLAGS+=	-Wno-deprecated-declarations

MAN=	pmc.3
MAN+=	pmc_allocate.3
MAN+=	pmc_attach.3
MAN+=	pmc_capabilities.3
MAN+=	pmc_configure_logfile.3
MAN+=	pmc_disable.3
MAN+=	pmc_event_names_of_class.3
MAN+=	pmc_get_driver_stats.3
MAN+=	pmc_get_msr.3
MAN+=	pmc_init.3
MAN+=	pmc_name_of_capability.3
MAN+=	pmc_read.3
MAN+=	pmc_set.3
MAN+=	pmc_start.3
MAN+=	pmclog.3
MAN+=	pmc.soft.3

# PMC-dependent manual pages
MAN+=	pmc.atom.3
MAN+=	pmc.atomsilvermont.3
MAN+=	pmc.core.3
MAN+=	pmc.core2.3
MAN+=	pmc.corei7.3
MAN+=	pmc.corei7uc.3
MAN+=	pmc.haswell.3
MAN+=	pmc.haswelluc.3
MAN+=	pmc.haswellxeon.3
MAN+=	pmc.iaf.3
MAN+=	pmc.ivybridge.3
MAN+=	pmc.ivybridgexeon.3
MAN+=	pmc.k7.3
MAN+=	pmc.k8.3
MAN+=	pmc.sandybridge.3
MAN+=	pmc.sandybridgeuc.3
MAN+=	pmc.sandybridgexeon.3
MAN+=	pmc.tsc.3
MAN+=	pmc.ucf.3
MAN+=	pmc.westmere.3
MAN+=	pmc.westmereuc.3

MLINKS+= \
	pmc_allocate.3 pmc_release.3 \
	pmc_attach.3 pmc_detach.3 \
	pmc_capabilities.3 pmc_ncpu.3 \
	pmc_capabilities.3 pmc_npmc.3 \
	pmc_capabilities.3 pmc_pmcinfo.3 \
	pmc_capabilities.3 pmc_cpuinfo.3 \
	pmc_capabilities.3 pmc_width.3 \
	pmc_configure_logfile.3 pmc_flush_logfile.3 \
	pmc_configure_logfile.3 pmc_writelog.3 \
	pmc_disable.3 pmc_enable.3 \
	pmc_name_of_capability.3 pmc_name_of_class.3 \
	pmc_name_of_capability.3 pmc_name_of_cputype.3 \
	pmc_name_of_capability.3 pmc_name_of_disposition.3 \
	pmc_name_of_capability.3 pmc_name_of_event.3 \
	pmc_name_of_capability.3 pmc_name_of_mode.3 \
	pmc_name_of_capability.3 pmc_name_of_state.3 \
	pmc_read.3 pmc_rw.3 \
	pmc_read.3 pmc_write.3 \
	pmc_start.3 pmc_stop.3

MLINKS+= \
	pmclog.3 pmclog_open.3 \
	pmclog.3 pmclog_close.3 \
	pmclog.3 pmclog_feed.3 \
	pmclog.3 pmclog_read.3

.include <bsd.lib.mk>
