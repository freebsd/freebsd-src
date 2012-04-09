# $FreeBSD: src/tools/tools/vimage/Makefile,v 1.2.2.2.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	vimage
LDADD=	-ljail
DPADD=	${LIBJAIL}

WARNS?=	2
CFLAGS+= -I../../../sys

MAN=	vimage.8

BINDIR?=	/usr/sbin

.include <bsd.prog.mk>
