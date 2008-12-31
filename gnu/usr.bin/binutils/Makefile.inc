# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.9.6.1 2008/11/25 02:59:29 kensmith Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
