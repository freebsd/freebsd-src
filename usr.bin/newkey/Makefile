# $FreeBSD: src/usr.bin/newkey/Makefile,v 1.14.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

PROG=	newkey
SRCS=	newkey.c generic.c update.c
.if ${MK_NIS} != "no"
CFLAGS+= -DYP
.endif
MAN=	newkey.8
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
