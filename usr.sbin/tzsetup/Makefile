# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.17.34.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
