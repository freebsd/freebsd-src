# $FreeBSD$

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	6
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
