# $FreeBSD: src/usr.bin/enigma/Makefile,v 1.9.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD=	${LIBCRYPT}
LDADD=	-lcrypt

.include <bsd.prog.mk>
