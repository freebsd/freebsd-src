# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.10 1999/09/01 23:36:21 chris Exp $

PROG=	tzsetup
MAN8=	tzsetup.8
CFLAGS+=	-I${.CURDIR}
LDADD+=	-ldialog -lncurses -lmytinfo
DPADD+=	${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.include <bsd.prog.mk>
