# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.13 2004/07/30 21:43:21 njl Exp $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
