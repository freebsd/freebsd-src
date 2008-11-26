# $FreeBSD: src/usr.bin/chkey/Makefile,v 1.10.12.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../newkey

PROG=	chkey
SRCS=	chkey.c generic.c update.c
CFLAGS+= -I${.CURDIR}/../newkey
.if !defined(NO_NIS)
CFLAGS+= -DYP
.endif
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
