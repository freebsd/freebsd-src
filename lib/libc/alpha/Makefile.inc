# $FreeBSD: src/lib/libc/alpha/Makefile.inc,v 1.5 1999/08/27 23:58:07 peter Exp $
#
# Machine dependent definitions for the alpha architecture.
#

#
# Alpha is 64-bit, so it doesn't need quad functions:
#
NO_QUAD=1
