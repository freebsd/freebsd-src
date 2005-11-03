#
# $FreeBSD: src/usr.sbin/pmcstat/Makefile,v 1.3 2005/06/30 19:01:26 jkoshy Exp $
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBPMC} ${LIBM}
LDADD=	-lpmc -lm

WARNS?=	6

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c

.include <bsd.prog.mk>
