# $FreeBSD: src/lib/libc/powerpc/Makefile.inc,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
