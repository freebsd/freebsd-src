# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.9 2006/09/12 19:24:01 obrien Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
