# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.17.30.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
