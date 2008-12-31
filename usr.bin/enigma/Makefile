# $FreeBSD: src/usr.bin/enigma/Makefile,v 1.9.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD=	${LIBCRYPT}
LDADD=	-lcrypt

.include <bsd.prog.mk>
