# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.11 2003/04/04 17:49:12 obrien Exp $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	2
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
