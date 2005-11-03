# $FreeBSD: src/lib/libc/alpha/Makefile.inc,v 1.7 2005/01/15 05:23:56 das Exp $
#
# Machine dependent definitions for the alpha architecture.
#

#
# Alpha is 64-bit, so it doesn't need quad functions:
#
NO_QUAD=

# On Alpha, long double is just double precision.
MDSRCS+=machdep_ldisd.c
