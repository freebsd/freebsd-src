# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.16 2001/07/20 06:20:25 obrien Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

LDADD=	-ldialog -lncurses
DPADD=	${LIBDIALOG} ${LIBNCURSES}

.include <bsd.prog.mk>
