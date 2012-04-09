# $FreeBSD: src/lib/libc/arm/Makefile.inc,v 1.5.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/arm/Symbol.map
