# $FreeBSD: src/lib/libc/sparc64/stdlib/gdtoa.mk,v 1.1 2003/03/12 20:29:59 das Exp $

# Long double is quad precision
GDTOASRCS+=strtopQ.c
MDSRCS+=machdep_ldisQ.c
