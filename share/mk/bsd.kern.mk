#	$Id: bsd.kern.mk,v 1.2.2.1 1996/11/12 08:01:32 phk Exp $

#
# Warning flags for compiling the kernel and components of the kernel.
#
CWARNFLAGS?=	-Wreturn-type -Wcomment -Wredundant-decls -Wimplicit \
		-Wnested-externs -Wstrict-prototypes -Wmissing-prototypes \
		-Wpointer-arith # -W -Winline -Wunused -Wcast-qual
#
# The following flags are next up for working on:
#	-Wformat -Wall
#
# When working on removing warnings from code, the `-Werror' flag should be
# of material assistance.
#
