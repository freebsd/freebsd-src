#
# $FreeBSD$
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBPMC} ${LIBM}
LDADD=	-lpmc -lm

WARNS?=	6

SRCS=	pmcstat.c

.include <bsd.prog.mk>
