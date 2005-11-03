# $FreeBSD: src/lib/libc/sparc64/Makefile.inc,v 1.4 2005/01/15 05:23:58 das Exp $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

#
# sparc64 is 64-bit, so it doesn't need quad functions.
#
NO_QUAD=

# Long double is quad precision
GDTOASRCS+=strtopQ.c
MDSRCS+=machdep_ldisQ.c
