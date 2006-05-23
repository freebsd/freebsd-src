# $FreeBSD: src/usr.bin/newkey/Makefile,v 1.12.2.1 2004/11/28 14:10:19 bz Exp $

PROG=	newkey
SRCS=	newkey.c generic.c update.c
.if !defined(NO_NIS)
CFLAGS+= -DYP
.endif
MAN=	newkey.8
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
