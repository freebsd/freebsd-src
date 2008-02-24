# $FreeBSD: src/lib/libc/powerpc/Makefile.inc,v 1.2 2006/03/13 01:14:58 deischen Exp $

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
