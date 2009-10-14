# $FreeBSD$

PROG=	vimage
LDADD=	-ljail
DPADD=	${LIBJAIL}

WARNS?=	2
CFLAGS+= -I../../../sys

MAN=	vimage.8

BINDIR?=	/usr/sbin

.include <bsd.prog.mk>
