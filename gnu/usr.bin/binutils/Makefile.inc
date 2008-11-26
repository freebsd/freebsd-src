# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.8.8.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
