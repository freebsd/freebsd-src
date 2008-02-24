# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.17 2003/04/04 17:49:20 obrien Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
