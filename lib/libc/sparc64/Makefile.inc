# $FreeBSD$
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

#
# sparc64 is 64-bit, so it doesn't need quad functions.
#
NO_QUAD=1
