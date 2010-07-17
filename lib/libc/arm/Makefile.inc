# $FreeBSD: src/lib/libc/arm/Makefile.inc,v 1.5.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/arm/Symbol.map
