# $FreeBSD: src/lib/libc/i386/Makefile.inc,v 1.3.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/i386/Symbol.map
