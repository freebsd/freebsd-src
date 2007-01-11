# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.8.8.1 2006/11/06 02:20:27 obrien Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
