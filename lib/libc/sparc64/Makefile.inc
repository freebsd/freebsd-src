# $FreeBSD: src/lib/libc/sparc64/Makefile.inc,v 1.2.8.1 2005/02/13 07:23:12 obrien Exp $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

#
# sparc64 is 64-bit, so it doesn't need quad functions.
#
NO_QUAD=
