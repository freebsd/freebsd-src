#
# $FreeBSD: src/gnu/usr.bin/binutils/Makefile.inc,v 1.8 2004/02/24 19:23:28 johan Exp $
#

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
