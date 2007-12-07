#
# $FreeBSD$
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBKVM} ${LIBPMC} ${LIBM}
LDADD=	-lelf -lkvm -lpmc -lm

WARNS?=	6

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c

.include <bsd.prog.mk>
