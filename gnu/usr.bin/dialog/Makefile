# $FreeBSD$

.include <src.opts.mk>

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI=	pure
.endif

DIALOG=	${SRCTOP}/contrib/dialog
PROG=   dialog

LIBADD=	dialog ncursesw m
CFLAGS+= -I${.CURDIR} -I${DIALOG}
.PATH:	${DIALOG}

WARNS?= 6

.include <bsd.prog.mk>
