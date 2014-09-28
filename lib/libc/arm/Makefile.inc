# $FreeBSD$
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${LIBC_SRCTOP}/arm/Symbol.map

.if ${MK_ARM_EABI} == "no"
# This contains the symbols that were removed when moving to the ARM EABI
SYM_MAPS+=${LIBC_SRCTOP}/arm/Symbol_oabi.map
.else
.include "${LIBC_SRCTOP}/arm/aeabi/Makefile.inc"
.endif

.if ${MACHINE_ARCH} == "armv6hf"
SYM_MAPS+=${LIBC_SRCTOP}/arm/Symbol_vfp.map
.endif

