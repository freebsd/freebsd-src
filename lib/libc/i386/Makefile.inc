# $FreeBSD: src/lib/libc/i386/Makefile.inc,v 1.1.12.1 2008/10/02 02:57:24 kensmith Exp $

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
