# $FreeBSD: src/usr.bin/ncplogin/Makefile,v 1.5.34.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	ncplogin
MAN=	ncplogin.1 ncplogout.1

LINKS=	${BINDIR}/ncplogin ${BINDIR}/ncplogout

LDADD=	-lncp -lipx
DPADD=	${LIBNCP} ${LIBIPX}

.include <bsd.prog.mk>
