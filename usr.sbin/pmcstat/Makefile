#
# $FreeBSD: src/usr.sbin/pmcstat/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBKVM} ${LIBPMC} ${LIBM}
LDADD=	-lkvm -lpmc -lm

WARNS?=	6

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c

.include <bsd.prog.mk>
