# $FreeBSD$

PROG=	tzsetup
MAN=	tzsetup.8
CFLAGS+= -I${.CURDIR}
LDADD+=	-ldialog -lncurses
DPADD+=	${LIBDIALOG} ${LIBNCURSES}

.include <bsd.prog.mk>
