#	$Id: bsd.kern.mk,v 1.5 1997/02/22 13:56:10 peter Exp $

#
# Warning flags for compiling the kernel and components of the kernel.
#
CWARNFLAGS?=	-Wreturn-type -Wcomment -Wredundant-decls -Wimplicit \
		-Wnested-externs -Wstrict-prototypes -Wmissing-prototypes \
		-Wpointer-arith -Winline -Wuninitialized # -W -Wunused \
		-Wcast-qual
#
# The following flags are next up for working on:
#	-Wformat -Wall
#
# When working on removing warnings from code, the `-Werror' flag should be
# of material assistance.
#
