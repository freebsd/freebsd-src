#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.include <src.opts.mk>

BINDIR?=	/usr/libexec

WARNS?=		6
WFORMAT?=	1

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI?=pure
.endif
