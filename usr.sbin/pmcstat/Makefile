#
# $FreeBSD: src/usr.sbin/pmcstat/Makefile,v 1.5.2.1.2.1 2010/02/10 00:26:20 kensmith Exp $
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBELF} ${LIBKVM} ${LIBPMC} ${LIBM}
LDADD=	-lelf -lkvm -lpmc -lm

WARNS?=	6

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c

.include <bsd.prog.mk>
