#
# $FreeBSD: src/usr.sbin/pmcstat/Makefile,v 1.7.2.3.2.1 2010/06/14 02:09:06 kensmith Exp $
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBELF} ${LIBKVM} ${LIBPMC} ${LIBM} ${LIBNCURSES}
LDADD=	-lelf -lkvm -lpmc -lm -lncurses

WARNS?=	6

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c \
pmcpl_callgraph.c pmcpl_gprof.c pmcpl_annotate.c pmcpl_calltree.c

.include <bsd.prog.mk>
