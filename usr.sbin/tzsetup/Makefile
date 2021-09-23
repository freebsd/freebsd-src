# $FreeBSD$

.include <src.opts.mk>

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I.

.if ${MK_DIALOG} != no && !defined(BOOTSTRAPPING)
WARNS?=	3
CFLAGS+=	-I${SRCTOP}/contrib/dialog -DHAVE_DIALOG
LIBADD=	dialog tinfow
.endif

.include <bsd.prog.mk>
