# $FreeBSD$

#
# Warning flags for compiling the kernel and components of the kernel.
#
# Note that the newly added -Wcast-qual is responsible for generating 
# most of the remaining warnings.  Warnings introduced with -Wall will
# also pop up, but are easier to fix.
.if ${CC} == "icc"
#CWARNFLAGS=	-w2	# use this if you are terribly bored
CWARNFLAGS=
.else
CWARNFLAGS?=	-Wall -Wredundant-decls -Wnested-externs -Wstrict-prototypes \
		-Wmissing-prototypes -Wpointer-arith -Winline -Wcast-qual \
		-fformat-extensions -std=c99
.endif
#
# The following flags are next up for working on:
#	-W

#
# On the i386, do not align the stack to 16-byte boundaries.  Otherwise GCC
# 2.95 adds code to the entry and exit point of every function to align the
# stack to 16-byte boundaries -- thus wasting approximately 12 bytes of stack
# per function call.  While the 16-byte alignment may benefit micro benchmarks, 
# it is probably an overall loss as it makes the code bigger (less efficient
# use of code cache tag lines) and uses more stack (less efficient use of data
# cache tag lines).  Explicitly prohibit the use of SSE and other SIMD
# operations inside the kernel itself.  These operations are exclusively
# reserved for user applications.
#
.if ${MACHINE_ARCH} == "i386" && ${CC} != "icc"
CFLAGS+=	-mno-align-long-strings -mpreferred-stack-boundary=2 \
		-mno-mmx -mno-3dnow -mno-sse -mno-sse2
INLINE_LIMIT?=	8000
.endif

#
# On the alpha, make sure that we don't use floating-point registers and
# allow the use of BWX etc instructions (only needed for low-level i/o).
# Also, reserve register t7 to point at per-cpu global variables.
#
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=	-mno-fp-regs -ffixed-8 -Wa,-mev6
INLINE_LIMIT?=	15000
.endif

.if ${MACHINE_ARCH} == "arm"
INLINE_LIMIT?=	8000
.endif
#
# For IA-64, we use r13 for the kernel globals pointer and we only use
# a very small subset of float registers for integer divides.
#
.if ${MACHINE_ARCH} == "ia64"
CFLAGS+=	-ffixed-r13 -mfixed-range=f32-f127 -mno-sdata
INLINE_LIMIT?=	15000
.endif

#
# For sparc64 we want medlow code model, and we tell gcc to use floating
# point emulation.  This avoids using floating point registers for integer
# operations which it has a tendency to do.
#
.if ${MACHINE_ARCH} == "sparc64"
CFLAGS+=	-mcmodel=medlow -msoft-float
INLINE_LIMIT?=	15000
.endif

#
# For AMD64, we explicitly prohibit the use of FPU, SSE and other SIMD
# operations inside the kernel itself.  These operations are exclusively
# reserved for user applications.
#
.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-mcmodel=kernel -mno-red-zone \
		-mfpmath=387 -mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
		-msoft-float -fno-asynchronous-unwind-tables
INLINE_LIMIT?=	8000
.endif

#
# For PowerPC we tell gcc to use floating point emulation.  This avoids using
# floating point registers for integer operations which it has a tendency to do.
#
.if ${MACHINE_ARCH} == "powerpc"
CFLAGS+=	-msoft-float
INLINE_LIMIT?=	15000
.endif

#
# GCC 3.0 and above like to do certain optimizations based on the
# assumption that the program is linked against libc.  Stop this.
#
.if ${CC} == "icc"
CFLAGS+=	-nolib_inline
.else
CFLAGS+=	-ffreestanding
.endif

.if ${CC} == "icc"
CFLAGS+=	-restrict
.endif
