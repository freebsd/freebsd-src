# $FreeBSD$
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
