# $FreeBSD$

.include <src.opts.mk>

BINDIR=	/usr/libexec

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI?=pure
.endif

.include "../Makefile.inc"
