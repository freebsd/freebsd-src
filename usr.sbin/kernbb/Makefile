# $FreeBSD$

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

