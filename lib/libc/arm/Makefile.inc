# $FreeBSD$
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

CFLAGS+=-DSOFTFLOAT

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
