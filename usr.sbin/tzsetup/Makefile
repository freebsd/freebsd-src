# $FreeBSD$

PROG=	tzsetup
MAN=	tzsetup.8
CFLAGS+= -Wall -I${.CURDIR}
LDADD+=	-ldialog -lncurses
DPADD+=	${LIBDIALOG} ${LIBNCURSES}

.include <bsd.prog.mk>
