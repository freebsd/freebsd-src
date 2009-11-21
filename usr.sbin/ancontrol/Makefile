# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.13.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
