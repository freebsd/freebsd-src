# $FreeBSD: src/lib/libc/i386/Makefile.inc,v 1.2 2006/03/13 01:14:57 deischen Exp $

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/i386/Symbol.map
