#
# $FreeBSD: src/usr.sbin/pmcstat/Makefile,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBKVM} ${LIBPMC} ${LIBM}
LDADD=	-lkvm -lpmc -lm

WARNS?=	6

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c

.include <bsd.prog.mk>
