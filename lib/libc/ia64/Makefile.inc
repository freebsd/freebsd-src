# $FreeBSD: src/lib/libc/ia64/Makefile.inc,v 1.4.12.1 2008/10/02 02:57:24 kensmith Exp $
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
