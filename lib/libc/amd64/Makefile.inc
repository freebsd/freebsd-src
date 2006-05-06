# $FreeBSD: src/lib/libc/amd64/Makefile.inc,v 1.3 2005/01/15 05:23:57 das Exp $
#
# Machine dependent definitions for the amd64 architecture.
#

#
# AMD64 is 64-bit, so it doesn't need quad functions:
#
NO_QUAD=

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
