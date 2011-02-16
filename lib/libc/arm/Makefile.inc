# $FreeBSD: src/lib/libc/arm/Makefile.inc,v 1.4.12.1 2010/12/21 17:10:29 kensmith Exp $
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

CFLAGS+=-DSOFTFLOAT

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/arm/Symbol.map
