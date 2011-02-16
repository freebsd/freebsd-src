# $FreeBSD: src/usr.bin/chkey/Makefile,v 1.12.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

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
