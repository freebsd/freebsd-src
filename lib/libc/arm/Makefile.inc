# $FreeBSD: src/lib/libc/arm/Makefile.inc,v 1.3 2005/03/20 00:53:52 cognet Exp $
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

CFLAGS+=-DSOFTFLOAT

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
