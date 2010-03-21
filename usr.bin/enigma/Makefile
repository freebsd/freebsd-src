# $FreeBSD: src/usr.bin/enigma/Makefile,v 1.9.32.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD=	${LIBCRYPT}
LDADD=	-lcrypt

.include <bsd.prog.mk>
