# $FreeBSD: src/lib/libc/powerpc/Makefile.inc,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
