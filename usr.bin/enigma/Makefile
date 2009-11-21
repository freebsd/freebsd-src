# $FreeBSD: src/usr.bin/enigma/Makefile,v 1.9.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD=	${LIBCRYPT}
LDADD=	-lcrypt

.include <bsd.prog.mk>
