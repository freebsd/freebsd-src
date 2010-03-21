# $FreeBSD: src/lib/libc/powerpc/Makefile.inc,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
