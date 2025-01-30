# $NetBSD: varparse-dynamic.mk,v 1.10 2025/01/11 21:21:34 rillig Exp $

# Before 2020-07-27, there was an off-by-one error in Var_Parse that skipped
# the last character in the variable name.
# To trigger the bug, the variable had to be undefined.
.if ${.TARGET}			# exact match, may be undefined
.endif
# expect+1: Variable ".TARGEX" is undefined
.if ${.TARGEX}			# 1 character difference, must be defined
.endif
# expect+1: Variable ".TARGXX" is undefined
.if ${.TARGXX}			# 2 characters difference, must be defined
.endif

# When a dynamic variable (such as .TARGET) is evaluated in the global
# scope, it is not yet ready to be expanded.  Therefore the complete
# expression is returned as the variable value, hoping that it can be
# resolved at a later point.
#
# This test covers the code in Var_Parse that deals with DEF_UNDEF but not
# DEF_DEFINED for dynamic variables.
.if ${.TARGET:S,^,,} != "\${.TARGET:S,^,,}"
.  error
.endif

# If a dynamic variable is expanded in a non-local scope, the expression
# based on this variable is not expanded.  But there may be nested
# expressions in the modifiers, and these are kept unexpanded as well.
.if ${.TARGET:M${:Ufallback}} != "\${.TARGET:M\${:Ufallback}}"
.  error
.endif
.if ${.TARGET:M${UNDEF}} != "\${.TARGET:M\${UNDEF}}"
.  error
.endif
