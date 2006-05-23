# $FreeBSD: src/usr.bin/chkey/Makefile,v 1.9.2.1 2004/11/28 14:10:18 bz Exp $

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
