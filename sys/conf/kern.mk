#	$Id: bsd.kern.mk,v 1.9 1998/05/01 18:13:37 bde Exp $

#
# Warning flags for compiling the kernel and components of the kernel.
#
CWARNFLAGS?=	-Wreturn-type -Wcomment -Wredundant-decls -Wimplicit \
		-Wnested-externs -Wstrict-prototypes -Wmissing-prototypes \
		-Wpointer-arith -Winline -Wuninitialized -Wformat \
		-fformat-extensions -ansi
#
# The following flags are next up for working on:
#	-W -Wunused -Wcast-qual -Wall
#
# When working on removing warnings from code, the `-Werror' flag should be
# of material assistance.
#
