# $FreeBSD$

PROG=	infokey
SRCS=	infokey.c key.c

DPADD=	${LIBTXI}
LDADD=	${LIBTXI}

NO_PIE=	yes

.include <bsd.prog.mk>

.PATH:	${TXIDIR}/info ${TXIDIR}/doc
