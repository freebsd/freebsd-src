# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.13.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
