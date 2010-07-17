# $FreeBSD: src/lib/libc/i386/Makefile.inc,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/i386/Symbol.map
