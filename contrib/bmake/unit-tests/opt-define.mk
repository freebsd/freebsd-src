# $NetBSD: opt-define.mk,v 1.3 2022/01/23 16:09:38 rillig Exp $
#
# Tests for the -D command line option, which defines global variables to the
# value 1, like in the C preprocessor.

.MAKEFLAGS: -DVAR

# The variable has the exact value "1", not "1.0".
.if ${VAR} != "1"
.  error
.endif

# The variable can be overwritten by assigning another value to it.  This
# would not be possible if the variable had been specified on the command line
# as 'VAR=1' instead of '-DVAR'.
VAR=		overwritten
.if ${VAR} != "overwritten"
.  error
.endif

# The variable can be undefined.  If the variable had been defined in the
# "Internal" scope instead, undefining it would have no effect.
.undef VAR
.if defined(VAR)
.  error
.endif

all: .PHONY
