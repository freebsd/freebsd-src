# $FreeBSD: src/usr.sbin/kernbb/Makefile,v 1.10 2003/12/03 07:43:03 phk Exp $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

