#
# $FreeBSD$
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBPMC}
LDADD=	-lpmc -lm

WARNS=	6

CFLAGS+=	-I${.CURDIR}/../../sys -I${.CURDIR}/../../lib/libpmc

SRCS=	pmcstat.c

.include <bsd.prog.mk>
