# $FreeBSD: src/usr.bin/newkey/Makefile,v 1.15.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.include <bsd.own.mk>

PROG=	newkey
SRCS=	newkey.c generic.c update.c
.if ${MK_NIS} != "no"
CFLAGS+= -DYP
.endif
MAN=	newkey.8
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

WARNS?=	6

.include <bsd.prog.mk>
