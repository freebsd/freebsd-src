# $FreeBSD: src/libexec/atrun/Makefile,v 1.7.2.1 2000/08/07 11:14:54 ps Exp $

MAINSRC=${.CURDIR}/../../usr.bin/at

.include "${MAINSRC}/Makefile.inc"

PROG=	atrun
MAN8=	atrun.8
SRCS=	atrun.c gloadavg.c

BINDIR=	${ATLIB_DIR}
CLEANFILES= ${MAN8}

CFLAGS+=-I${MAINSRC} -I${.CURDIR}

${MAN8}: atrun.man
	sed -e \
		"s@_ATSPOOL_DIR@$(ATSPOOL_DIR)@g; \
		s@_ATJOB_DIR@$(ATJOB_DIR)@g; \
		s@_ATLIB_DIR@$(ATLIB_DIR)@g; \
		s@_LOADAVG_MX@$(LOADAVG_MX)@g;" \
		< ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
