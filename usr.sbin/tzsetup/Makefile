# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.17.38.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
