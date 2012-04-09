# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.13.30.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
