#
# $FreeBSD$
#

PROG=	pmccontrol
MAN=	pmccontrol.8

DPADD=	${LIBPMC}
LDADD=	-lpmc

WARNS=	6

CFLAGS+=	-I${.CURDIR}/../../sys -I${.CURDIR}/../../lib/libpmc

SRCS=	pmccontrol.c

.include <bsd.prog.mk>
