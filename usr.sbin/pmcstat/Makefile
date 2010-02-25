#
# $FreeBSD$
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBELF} ${LIBKVM} ${LIBPMC} ${LIBM} ${LIBNCURSES}
LDADD=	-lelf -lkvm -lpmc -lm -lncurses

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c \
pmcpl_callgraph.c pmcpl_gprof.c pmcpl_annotate.c pmcpl_calltree.c

.include <bsd.prog.mk>
