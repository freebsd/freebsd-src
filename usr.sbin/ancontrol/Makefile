# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.13.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
