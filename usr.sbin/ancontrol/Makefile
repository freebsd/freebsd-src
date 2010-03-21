# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.13.32.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
