#	$Id: bsd.kern.mk,v 1.11 1998/12/14 21:03:27 archie Exp $

#
# Warning flags for compiling the kernel and components of the kernel.
#
CWARNFLAGS?=	-Wreturn-type -Wcomment -Wredundant-decls -Wimplicit \
		-Wnested-externs -Wstrict-prototypes -Wmissing-prototypes \
		-Wpointer-arith -Winline -Wuninitialized -Wformat -Wunused \
		-fformat-extensions -ansi
#
# The following flags are next up for working on:
#	-W -Wcast-qual -Wall
#
# When working on removing warnings from code, the `-Werror' flag should be
# of material assistance.
#

#
# On the alpha, make sure that we don't use floating-point registers and
# allow the use of EV56 instructions (only needed for low-level i/o).
#
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=	-mno-fp-regs -Wa,-mev56
.endif
