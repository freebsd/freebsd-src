# $NetBSD: varname-makefile.mk,v 1.3 2020/11/09 22:36:44 rillig Exp $
#
# Tests for the special MAKEFILE variable, which contains the current
# makefile from the -f command line option.
#
# When there are multiple -f options, the variable MAKEFILE is set
# again for each of these makefiles, before the file is parsed.
# Including a file via .include does not influence the MAKEFILE
# variable though.

.if ${MAKEFILE:T} != "varname-makefile.mk"
.  error
.endif

# This variable lives in the "Internal" namespace.
# TODO: Why does it do that, and what consequences does this have?

# Deleting the variable does not work since this variable does not live in
# the "Global" namespace but in "Internal", which is kind of a child
# namespace.
#
.undef MAKEFILE
.if ${MAKEFILE:T} != "varname-makefile.mk"
.  error
.endif

# Overwriting this variable is possible since the "Internal" namespace
# serves as a fallback for the "Global" namespace (see VarFind).
#
MAKEFILE=	overwritten
.if ${MAKEFILE:T} != "overwritten"
.  error
.endif

# When the overwritten value is deleted, the fallback value becomes
# visible again.
#
.undef MAKEFILE
.if ${MAKEFILE:T} != "varname-makefile.mk"
.  error
.endif

all:
	# MAKEFILE is the file that appeared last in the command line.
	: In the end, MAKEFILE is ${MAKEFILE}.

# Additional makefiles can be added while reading a makefile.  They will be
# read in order.
.MAKEFLAGS: -f /dev/null
