# $FreeBSD: src/lib/libc/i386/Makefile.inc,v 1.3.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/i386/Symbol.map
