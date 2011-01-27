# $FreeBSD$

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}/../../gnu/lib/libodialog -I.

WARNS?=	3

DPADD=	${LIBODIALOG} ${LIBNCURSES}
LDADD=	-lodialog -lncurses

.include <bsd.prog.mk>
