# $FreeBSD$
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
