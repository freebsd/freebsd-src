# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.17.32.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
