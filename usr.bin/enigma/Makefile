# $FreeBSD: src/usr.bin/enigma/Makefile,v 1.9.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD=	${LIBCRYPT}
LDADD=	-lcrypt

.include <bsd.prog.mk>
