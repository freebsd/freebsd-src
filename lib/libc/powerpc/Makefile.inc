# $FreeBSD: src/lib/libc/powerpc/Makefile.inc,v 1.2.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
