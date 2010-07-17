# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.17.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
