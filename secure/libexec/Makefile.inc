# $FreeBSD$

.include <src.opts.mk>

BINDIR?=	/usr/libexec
PACKAGE=	ssh

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI?=pure
.endif

.include "../Makefile.inc"
