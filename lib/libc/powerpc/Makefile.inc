# $FreeBSD: src/lib/libc/powerpc/Makefile.inc,v 1.2.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
