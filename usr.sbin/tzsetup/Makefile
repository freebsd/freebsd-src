# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.17.34.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
