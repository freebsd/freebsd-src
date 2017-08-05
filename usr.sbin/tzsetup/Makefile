# $FreeBSD$

.include <bsd.own.mk>

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I.

.if ${MK_DIALOG} != no
WARNS?=	3
CFLAGS+=	-I${.CURDIR}/../../contrib/dialog -DHAVE_DIALOG
DPADD=	${LIBDIALOG} ${LIBM}
LDADD=	-ldialog -lm
.endif

.if ${MK_NCURSESW} == "no"
DPADD+=	${LIBNCURSES}
LDADD+=	-lncurses
.else
DPADD+=	${LIBNCURSESW}
LDADD+= -lncursesw
.endif

.include <bsd.prog.mk>
