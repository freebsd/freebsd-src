# $FreeBSD: src/usr.bin/ncplogin/Makefile,v 1.5.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	ncplogin
MAN=	ncplogin.1 ncplogout.1

LINKS=	${BINDIR}/ncplogin ${BINDIR}/ncplogout

LDADD=	-lncp -lipx
DPADD=	${LIBNCP} ${LIBIPX}

.include <bsd.prog.mk>
