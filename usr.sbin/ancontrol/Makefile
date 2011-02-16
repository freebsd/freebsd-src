# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.13.34.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
