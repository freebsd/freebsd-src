# $FreeBSD$
#
# Common definitons for programs building in the stand-alone environment
# and/or using libstand.
#

CFLAGS+= -ffreestanding -Wformat
CFLAGS+= ${CFLAGS_NO_SIMD} -D_STANDALONE
.if ${MACHINE_CPUARCH} == "riscv"
CFLAGS+=	-mno-float
.elif ${MACHINE_CPUARCH} != "aarch64"
CFLAGS+=	-msoft-float
.endif

.if ${MACHINE_CPUARCH} == "i386"
CFLAGS.gcc+=	-mpreferred-stack-boundary=2
.endif
.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-fPIC -mno-red-zone
.endif
.if ${MACHINE_CPUARCH} == "aarch64"
CFLAGS+=	-fPIC -mgeneral-regs-only
.endif
.if ${MACHINE_CPUARCH} == "mips"
CFLAGS+=	-G0 -fno-pic -mno-abicalls
.endif
