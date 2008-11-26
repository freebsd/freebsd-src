# $FreeBSD: src/lib/libc/alpha/Makefile.inc,v 1.7.12.1 2008/10/02 02:57:24 kensmith Exp $
#
# Machine dependent definitions for the alpha architecture.
#

#
# Alpha is 64-bit, so it doesn't need quad functions:
#
NO_QUAD=

# On Alpha, long double is just double precision.
MDSRCS+=machdep_ldisd.c
