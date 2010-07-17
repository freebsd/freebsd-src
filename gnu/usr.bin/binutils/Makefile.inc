# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.9.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
