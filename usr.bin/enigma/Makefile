# $FreeBSD$

PROG=	enigma

LINKS=	${BINDIR}/enigma ${BINDIR}/crypt
MLINKS=	enigma.1 crypt.1

DPADD+=	${LIBCRYPT}
LDADD+=	-lcrypt

.include <bsd.prog.mk>
