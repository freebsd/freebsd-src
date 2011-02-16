# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.9.14.1 2010/12/21 17:10:29 kensmith Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
