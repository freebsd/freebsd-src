# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.13.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
