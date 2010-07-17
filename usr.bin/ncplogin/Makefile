# $FreeBSD: src/usr.bin/ncplogin/Makefile,v 1.5.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	ncplogin
MAN=	ncplogin.1 ncplogout.1

LINKS=	${BINDIR}/ncplogin ${BINDIR}/ncplogout

LDADD=	-lncp -lipx
DPADD=	${LIBNCP} ${LIBIPX}

.include <bsd.prog.mk>
