# $FreeBSD: src/lib/libc/sparc64/Makefile.inc,v 1.4.12.1 2008/10/02 02:57:24 kensmith Exp $
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
