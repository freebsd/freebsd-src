# $FreeBSD$

PROG=		ancontrol
MAN=		ancontrol.8

CFLAGS+=	-I${.CURDIR}/../../sys -DANCACHE
DPADD=		${LIBMD}
LDADD=		-lmd

WARNS?=		2

.include <bsd.prog.mk>
