# $FreeBSD: src/usr.bin/enigma/Makefile,v 1.9 2004/02/05 22:44:24 ru Exp $

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD=	${LIBCRYPT}
LDADD=	-lcrypt

.include <bsd.prog.mk>
