# $FreeBSD: src/lib/libc/ia64/Makefile.inc,v 1.4 2005/01/15 05:23:58 das Exp $
#
# Machine dependent definitions for the alpha architecture.
#

#
# IA-64 is 64-bit, so it doesn't need quad functions:
#
NO_QUAD=

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
