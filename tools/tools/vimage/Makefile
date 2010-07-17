# $FreeBSD: src/tools/tools/vimage/Makefile,v 1.2.2.2.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	vimage
LDADD=	-ljail
DPADD=	${LIBJAIL}

WARNS?=	2
CFLAGS+= -I../../../sys

MAN=	vimage.8

BINDIR?=	/usr/sbin

.include <bsd.prog.mk>
