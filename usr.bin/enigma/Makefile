# $FreeBSD: src/usr.bin/enigma/Makefile,v 1.9.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD=	${LIBCRYPT}
LDADD=	-lcrypt

.include <bsd.prog.mk>
