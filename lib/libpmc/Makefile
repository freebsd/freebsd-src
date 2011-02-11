# $FreeBSD$

LIB=	pmc

SRCS=	libpmc.c pmclog.c
INCS=	pmc.h pmclog.h

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

# PMC-dependent manual pages
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
MAN+=	pmc.atom.3
MAN+=	pmc.core.3
MAN+=	pmc.core2.3
MAN+=	pmc.iaf.3
MAN+=	pmc.ucf.3
MAN+=	pmc.k7.3
MAN+=	pmc.k8.3
MAN+=	pmc.p4.3
MAN+=	pmc.p5.3
MAN+=	pmc.p6.3
MAN+=	pmc.corei7.3
MAN+=	pmc.corei7uc.3
MAN+=	pmc.westmere.3
MAN+=	pmc.westmereuc.3
MAN+=	pmc.tsc.3
.elif ${MACHINE_CPUARCH} == "arm" && defined(CPUTYPE) && ${CPUTYPE} == "xscale"
MAN+=	pmc.xscale.3
.endif

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
