# $FreeBSD: src/lib/libc/alpha/stdlib/gdtoa.mk,v 1.1 2003/03/12 20:29:58 das Exp $

# On Alpha, long double is just double precision.
MDSRCS+=machdep_ldisd.c
