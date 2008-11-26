#
# $FreeBSD: src/usr.sbin/pmcstat/Makefile,v 1.3.12.1 2008/10/02 02:57:24 kensmith Exp $
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBPMC} ${LIBM}
LDADD=	-lpmc -lm

WARNS?=	6

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c

.include <bsd.prog.mk>
