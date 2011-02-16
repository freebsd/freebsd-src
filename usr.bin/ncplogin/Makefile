# $FreeBSD: src/usr.bin/ncplogin/Makefile,v 1.5.40.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	ncplogin
MAN=	ncplogin.1 ncplogout.1

LINKS=	${BINDIR}/ncplogin ${BINDIR}/ncplogout

LDADD=	-lncp -lipx
DPADD=	${LIBNCP} ${LIBIPX}

.include <bsd.prog.mk>
