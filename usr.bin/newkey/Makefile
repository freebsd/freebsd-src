# $FreeBSD: src/usr.bin/newkey/Makefile,v 1.10 2002/04/28 12:08:18 markm Exp $

PROG=	newkey
SRCS=	newkey.c generic.c update.c
CFLAGS+= -DYP
MAN=	newkey.8
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
