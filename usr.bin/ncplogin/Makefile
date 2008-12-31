# $FreeBSD: src/usr.bin/ncplogin/Makefile,v 1.5.32.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	ncplogin
MAN=	ncplogin.1 ncplogout.1

LINKS=	${BINDIR}/ncplogin ${BINDIR}/ncplogout

LDADD=	-lncp -lipx
DPADD=	${LIBNCP} ${LIBIPX}

.include <bsd.prog.mk>
