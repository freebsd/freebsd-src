# $FreeBSD: src/usr.bin/chkey/Makefile,v 1.11.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../newkey

PROG=	chkey
SRCS=	chkey.c generic.c update.c
CFLAGS+= -I${.CURDIR}/../newkey
.if ${MK_NIS} != "no"
CFLAGS+= -DYP
.endif
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
