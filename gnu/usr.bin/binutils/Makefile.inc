# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.9.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
