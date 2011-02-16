# $FreeBSD: src/tools/tools/vimage/Makefile,v 1.2.2.2.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	vimage
LDADD=	-ljail
DPADD=	${LIBJAIL}

WARNS?=	2
CFLAGS+= -I../../../sys

MAN=	vimage.8

BINDIR?=	/usr/sbin

.include <bsd.prog.mk>
