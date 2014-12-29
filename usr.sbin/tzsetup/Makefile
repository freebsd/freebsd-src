# $FreeBSD$

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}/../../contrib/dialog -I.

WARNS?=	3

LIBADD=	dialog ncursesw

.include <bsd.prog.mk>
