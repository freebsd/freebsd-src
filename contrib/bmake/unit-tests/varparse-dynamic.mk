# $NetBSD: varparse-dynamic.mk,v 1.3 2020/11/21 15:48:05 rillig Exp $

# Before 2020-07-27, there was an off-by-one error in Var_Parse that skipped
# the last character in the variable name.
# To trigger the bug, the variable must not be defined.
.if ${.TARGET}			# exact match, may be undefined
.endif
.if ${.TARGEX}			# 1 character difference, must be defined
.endif
.if ${.TARGXX}			# 2 characters difference, must be defined
.endif

# When a dynamic variable (such as .TARGET) is evaluated in the global
# context, it is not yet ready to be expanded.  Therefore the complete
# expression is returned as the variable value, hoping that it can be
# resolved at a later point.
#
# This test covers the code in Var_Parse that deals with VAR_JUNK but not
# VAR_KEEP for dynamic variables.
.if ${.TARGET:S,^,,} != "\${.TARGET:S,^,,}"
.  error
.endif

# If a dynamic variable is expanded in a non-local context, the expression
# based on this variable is not expanded.  But there may be nested variable
# expressions in the modifiers, and these are kept unexpanded as well.
.if ${.TARGET:M${:Ufallback}} != "\${.TARGET:M\${:Ufallback}}"
.  error
.endif
.if ${.TARGET:M${UNDEF}} != "\${.TARGET:M\${UNDEF}}"
.  error
.endif

all:
	@:
