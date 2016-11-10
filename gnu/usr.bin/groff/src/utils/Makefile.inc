# $FreeBSD$

.include <src.opts.mk>

TOPREL=		../../..
DIST_SUBDIR=	src/utils/${.CURDIR:T}

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI?=    pure
.endif

.include "../../Makefile.inc"
