# $FreeBSD: src/lib/libc/powerpc/Makefile.inc,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
