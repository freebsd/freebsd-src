# $FreeBSD$

.include <src.opts.mk>

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I.

.if !defined(BOOTSTRAPPING)
WARNS?=	3
CFLAGS+=	-I${SRCTOP}/contrib/bsddialog/lib -DHAVE_BSDDIALOG
LIBADD=		bsddialog
.endif

.include <bsd.prog.mk>
