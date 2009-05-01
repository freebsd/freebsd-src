# $FreeBSD: src/lib/libc/i386/Makefile.inc,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/i386/Symbol.map
