# $FreeBSD$
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/arm/Symbol.map

.if ${MK_ARM_EABI} == "no"
# This contains the symbols that were removed when moving to the ARM EABI
SYM_MAPS+=${.CURDIR}/arm/Symbol_oabi.map
.else
.include "${.CURDIR}/arm/aeabi/Makefile.inc"
.endif

