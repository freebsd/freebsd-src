# $FreeBSD: src/lib/libc/arm/Makefile.inc,v 1.3.12.1 2008/10/02 02:57:24 kensmith Exp $
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

CFLAGS+=-DSOFTFLOAT

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
