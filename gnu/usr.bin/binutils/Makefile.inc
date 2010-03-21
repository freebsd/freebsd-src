# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.9.12.1 2010/02/10 00:26:20 kensmith Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
