# $FreeBSD: src/lib/libc/alpha/Makefile.inc,v 1.4.2.1 1999/08/29 14:45:37 peter Exp $
#
# Machine dependent definitions for the alpha architecture.
#

#
# Alpha is 64-bit, so it doesn't need quad functions:
#
NO_QUAD=1
