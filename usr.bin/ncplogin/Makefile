# $FreeBSD$

PROG=	ncplogin
MAN=	ncplogin.1 ncplogout.1
WARNS?=	2

LINKS=	${BINDIR}/ncplogin ${BINDIR}/ncplogout

LDADD=	-lncp -lipx
DPADD=	${LIBNCP} ${LIBIPX}

.include <bsd.prog.mk>
