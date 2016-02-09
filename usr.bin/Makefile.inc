#	@(#)Makefile.inc	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

BINDIR?=	/usr/bin

WARNS?=		6

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI?=	pure
.endif
