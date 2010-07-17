# $FreeBSD: src/usr.bin/chkey/Makefile,v 1.12.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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

WARNS?=	6

.include <bsd.prog.mk>
