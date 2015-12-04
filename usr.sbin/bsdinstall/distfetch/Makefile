# $FreeBSD$

BINDIR= ${LIBEXECDIR}/bsdinstall
PROG=	distfetch
DPADD=	${LIBFETCH} ${LIBDIALOG} ${LIBM}
LDADD=	-lfetch -ldialog -lm

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
