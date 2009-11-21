# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.9.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
