# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.17.28.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
