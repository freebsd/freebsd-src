# $NetBSD: var-class-local.mk,v 1.4 2020/10/25 09:46:25 rillig Exp $
#
# Tests for target-local variables, such as ${.TARGET} or $@.

# TODO: Implementation

# Ensure that the name of the variable is exactly the given one.
# The variable "@" is an alias for ".TARGET", so the implementation might
# canonicalize these aliases at some point, and that might be surprising.
# This aliasing happens for single-character variable names like $@ or $<
# (see VarFind, CanonicalVarname), but not for braced or parenthesized
# expressions like ${@}, ${.TARGET} ${VAR:Mpattern} (see Var_Parse,
# ParseVarname).
.if ${@:L} != "@"
.  error
.endif
.if ${.TARGET:L} != ".TARGET"
.  error
.endif
.if ${@F:L} != "@F"
.  error
.endif
.if ${@D:L} != "@D"
.  error
.endif

all:
	# The ::= modifier overwrites the .TARGET variable in the node
	# 'all', not in the global scope.  This can be seen with the -dv
	# option, looking for "all:@ = overwritten".
	: ${.TARGET} ${.TARGET::=overwritten}${.TARGET}
