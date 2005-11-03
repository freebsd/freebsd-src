# $FreeBSD: src/usr.bin/newkey/Makefile,v 1.13 2004/11/13 20:40:30 bz Exp $

PROG=	newkey
SRCS=	newkey.c generic.c update.c
.if !defined(NO_NIS)
CFLAGS+= -DYP
.endif
MAN=	newkey.8
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
