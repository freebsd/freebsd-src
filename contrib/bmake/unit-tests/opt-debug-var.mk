# $NetBSD: opt-debug-var.mk,v 1.3 2023/11/19 21:47:52 rillig Exp $
#
# Tests for the -dv command line option, which adds debug logging about
# variable assignment and evaluation.

.MAKEFLAGS: -dv

# expect: Global: ASSIGNED = value
ASSIGNED=	value

# TODO: Explain why the empty assignment "Global: SUBST = " is needed.
# expect: Global: SUBST = value
SUBST:=		value

.if defined(ASSIGNED)
.endif

# The usual form of expressions is ${VAR}.  The form $(VAR) is used
# less often as it can be visually confused with the shell construct for
# capturing the output of a subshell, which looks the same.
#
# In conditions, a call to the function 'empty' is syntactically similar to
# the form $(VAR), only that the initial '$' is the 'y' of 'empty'.
#
# expect: Var_Parse: y(ASSIGNED) (eval)
.if !empty(ASSIGNED)
.endif

.MAKEFLAGS: -d0

all: .PHONY
