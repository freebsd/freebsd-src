#	$Id: bsd.kern.mk,v 1.1 1996/10/08 22:10:49 bde Exp $

#
# Warning flags for compiling the kernel and components of the kernel.
#
CWARNFLAGS?=	-W -Wreturn-type -Wcomment -Wredundant-decls -Wimplicit \
		-Wnested-externs -Wstrict-prototypes -Wmissing-prototypes \
		-Winline -Wpointer-arith # -Wunused -Wcast-qual
#
# The following flags are next up for working on:
#	-Wformat -Wall
#
# When working on removing warnings from code, the `-Werror' flag should be
# of material assistance.
#
