# $FreeBSD$

PROG=	tzsetup
CFLAGS+=	-I${.CURDIR}
LDADD+=	-ldialog -lncurses
DPADD+=	${LIBDIALOG} ${LIBNCURSES}

.include <bsd.prog.mk>
