# $FreeBSD: src/usr.bin/newkey/Makefile,v 1.13.12.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	newkey
SRCS=	newkey.c generic.c update.c
.if !defined(NO_NIS)
CFLAGS+= -DYP
.endif
MAN=	newkey.8
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
