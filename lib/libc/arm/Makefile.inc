# $FreeBSD: src/lib/libc/arm/Makefile.inc,v 1.4.8.1 2009/04/15 03:14:26 kensmith Exp $
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

CFLAGS+=-DSOFTFLOAT

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/arm/Symbol.map
