#
# $FreeBSD: src/usr.sbin/pmcstat/Makefile,v 1.5 2007/10/01 18:15:11 ru Exp $
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBKVM} ${LIBPMC} ${LIBM}
LDADD=	-lkvm -lpmc -lm

WARNS?=	6

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c

.include <bsd.prog.mk>
