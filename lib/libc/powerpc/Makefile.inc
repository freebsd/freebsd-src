# $FreeBSD: src/lib/libc/powerpc/Makefile.inc,v 1.2.14.1 2010/12/21 17:10:29 kensmith Exp $

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
