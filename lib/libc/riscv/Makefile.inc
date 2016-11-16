# $FreeBSD$
#
# Machine dependent definitions for the RISC-V architecture.
#

.if ${MACHINE_ARCH:Mriscv*sf} != ""
CFLAGS+=-DSOFTFLOAT
.endif

# Long double is quad precision
GDTOASRCS+=strtorQ.c
MDSRCS+=machdep_ldisQ.c
SYM_MAPS+=${LIBC_SRCTOP}/riscv/Symbol.map
