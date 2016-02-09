#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD$

.include <src.opts.mk>

BINDIR?=	/sbin
WARNS?=	6

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI?=pure
.endif

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
