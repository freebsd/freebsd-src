# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.9.8.1 2009/04/15 03:14:26 kensmith Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
