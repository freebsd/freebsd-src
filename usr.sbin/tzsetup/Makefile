# $FreeBSD$

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}/../../contrib/dialog -I.

WARNS?=	3

DPADD=	${LIBDIALOG} ${LIBNCURSESW} ${LIBM}
LDADD=	-ldialog -lncursesw -lm

.include <bsd.prog.mk>
