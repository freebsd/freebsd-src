# $FreeBSD$
PACKAGE=lib${LIB}
LIB=	pmcstat
INTERNALLIB=

SRCS=	\
	libpmcstat_event.c	\
	libpmcstat_image.c	\
	libpmcstat_logging.c	\
	libpmcstat_process.c	\
	libpmcstat_string.c	\
	libpmcstat_symbol.c	\
	libpmcstat_pmu_util.c
INCS=	libpmcstat.h

CFLAGS+= -I${.CURDIR}

.if ${MACHINE_CPUARCH} == "amd64" 

.if ${MACHINE_CPUARCH} == "aarch64"
EVENT_ARCH="arm64"
.elif ${MACHINE_CPUARCH} == "amd64"
EVENT_ARCH="x86"
.elif ${MACHINE_CPUARCH} == "powerpc"
EVENT_ARCH="powerpc"
.endif

JEVENTS= ${BTOOLSPATH:U.}/pmu-events/jevents

libpmcstat_events.c: ${JEVENTS}
	${JEVENTS} ${EVENT_ARCH} ${.CURDIR}/pmu-events/arch libpmcstat_events.c
SRCS+= libpmcstat_events.c
.endif

.include <bsd.lib.mk>
