# $FreeBSD: src/lib/libc/i386/Makefile.inc,v 1.2.10.1 2010/02/10 00:26:20 kensmith Exp $

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/i386/Symbol.map
