# $FreeBSD: src/usr.bin/enigma/Makefile,v 1.8 2003/10/09 10:00:53 tjr Exp $

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD+=	${LIBCRYPT}
LDADD+=	-lcrypt

.include <bsd.prog.mk>
