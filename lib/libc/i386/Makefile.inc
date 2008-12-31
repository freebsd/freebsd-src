# $FreeBSD: src/lib/libc/i386/Makefile.inc,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/i386/Symbol.map
