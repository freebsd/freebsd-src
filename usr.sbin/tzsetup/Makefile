# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.7.2.1 1999/08/29 15:48:49 peter Exp $

PROG=	tzsetup
MAN8=	tzsetup.8
CFLAGS+=	-I${.CURDIR}
LDADD+=	-ldialog -lncurses -lmytinfo
DPADD+=	${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.include <bsd.prog.mk>
