# $FreeBSD: src/lib/libc/sparc64/Makefile.inc,v 1.2 2002/03/05 00:00:40 tmm Exp $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

#
# sparc64 is 64-bit, so it doesn't need quad functions.
#
NO_QUAD=1
