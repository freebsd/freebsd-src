# $FreeBSD: src/usr.bin/newkey/Makefile,v 1.12 2004/08/06 07:27:06 cperciva Exp $

PROG=	newkey
SRCS=	newkey.c generic.c update.c
CFLAGS+= -DYP
MAN=	newkey.8
DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBCRYPTO}
LDADD=	-lrpcsvc -lmp -lcrypto

.include <bsd.prog.mk>
