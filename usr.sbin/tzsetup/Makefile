# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.17.36.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
