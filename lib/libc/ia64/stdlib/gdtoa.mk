# $FreeBSD: src/lib/libc/ia64/stdlib/gdtoa.mk,v 1.1 2003/03/12 20:29:59 das Exp $

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
