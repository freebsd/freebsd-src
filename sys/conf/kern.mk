#	$Id: bsd.kern.mk,v 1.13 1999/01/27 22:53:58 dillon Exp $

#
# Warning flags for compiling the kernel and components of the kernel.
#
#	Note that the newly added -Wcast-qual is responsible for generating 
#	most of the remaining warnings.  Warnings introduced with -Wall
#	will also pop up, but are easier to fix.

CWARNFLAGS?=    -Wall -Wredundant-decls -Wnested-externs -Wstrict-prototypes \
                -Wmissing-prototypes -Wpointer-arith -Winline -Wcast-qual \
		-fformat-extensions -ansi

#
# On the alpha, make sure that we don't use floating-point registers and
# allow the use of EV56 instructions (only needed for low-level i/o).
#
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=	-mno-fp-regs -Wa,-mev56
.endif
