# $FreeBSD: src/lib/libc/powerpc/Makefile.inc,v 1.2.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
