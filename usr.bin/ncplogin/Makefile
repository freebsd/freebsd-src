# $FreeBSD: src/usr.bin/ncplogin/Makefile,v 1.2 1999/11/01 02:44:09 bp Exp $

PROG=	ncplogin
MAN1=	ncplogin.1 ncplogout.1

LINKS=	${BINDIR}/ncplogin ${BINDIR}/ncplogout

LDADD=	-lncp -lipx
DPADD=	${LIBNCP} ${LIBIPX}

.include <bsd.prog.mk>
