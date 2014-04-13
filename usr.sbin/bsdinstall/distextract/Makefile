# $FreeBSD$

BINDIR= /usr/libexec/bsdinstall
PROG=	distextract
DPADD=	${LIBARCHIVE} ${LIBDIALOG} ${LIBM}
LDADD=	-larchive -ldialog -lm

WARNS?=	6
MAN=

.include <bsd.own.mk>

.if ${MK_NCURSESW} == "no"
DPADD+=	${LIBNCURSES}
LDADD+=	-lncurses
.else
DPADD+=	${LIBNCURSESW}
LDADD+=	-lncursesw
.endif

.include <bsd.prog.mk>
