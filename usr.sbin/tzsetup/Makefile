# $FreeBSD$

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

WARNS?=	3

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
