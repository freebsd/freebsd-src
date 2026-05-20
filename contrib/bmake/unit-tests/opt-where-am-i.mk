# $NetBSD: opt-where-am-i.mk,v 1.5 2026/02/10 22:33:36 sjg Exp $
#
# Tests for the -w command line option, which outputs the current directory
# at the beginning and end of running make.  This is useful when building
# large source trees that involve several nested make calls.

# The first "Entering directory" is missing since the below .MAKEFLAGS comes
# too late for it.
# We force MAKEOBJDIRPREFIX=/ to avoid looking elsewhere for .OBJDIR
.MAKEFLAGS: -w

all:
.if ${.CURDIR} != "/"
	@MAKE_OBJDIR_CHECK_WRITABLE=no MAKEOBJDIRPREFIX=/ \
	${MAKE} -r -f ${MAKEFILE:tA} -C /
.endif
