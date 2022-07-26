# $NetBSD: opt-define.mk,v 1.4 2022/06/12 14:27:06 rillig Exp $
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
# "Internal" or in the "Command" scope instead, undefining it would have no
# effect.
.undef VAR
.if defined(VAR)
.  error
.endif

# The C preprocessor allows to define a macro with a specific value.  Make
# behaves differently, it defines a variable with the name 'VAR=value' and the
# value 1.
.MAKEFLAGS: -DVAR=value
.if defined(VAR)
.  error
.endif
.if ${VAR=value} != "1"
.  error
.endif

all: .PHONY
