# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.9.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
