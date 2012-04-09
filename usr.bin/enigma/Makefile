# $FreeBSD: src/usr.bin/enigma/Makefile,v 1.9.30.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD=	${LIBCRYPT}
LDADD=	-lcrypt

.include <bsd.prog.mk>
