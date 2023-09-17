# $NetBSD: cond-token-var.mk,v 1.7 2023/06/01 20:56:35 rillig Exp $
#
# Tests for variable expressions in .if conditions.
#
# Note the fine distinction between a variable and a variable expression.
# A variable has a name and a value.  To access the value, one writes a
# variable expression of the form ${VAR}.  This is a simple variable
# expression.  Variable expressions can get more complicated by adding
# variable modifiers such as in ${VAR:Mpattern}.
#
# XXX: Strictly speaking, variable modifiers should be called expression
# modifiers instead since they only modify the expression, not the variable.
# Well, except for the assignment modifiers, these do indeed change the value
# of the variable.

DEF=	defined

# A defined variable may appear on either side of the comparison.
.if ${DEF} == ${DEF}
# expect+1: ok
.  info ok
.else
.  error
.endif

# A variable that appears on the left-hand side must be defined.
# expect+1: Malformed conditional (${UNDEF} == ${DEF})
.if ${UNDEF} == ${DEF}
.  error
.endif

# A variable that appears on the right-hand side must be defined.
# expect+1: Malformed conditional (${DEF} == ${UNDEF})
.if ${DEF} == ${UNDEF}
.  error
.endif

# A defined variable may appear as an expression of its own.
.if ${DEF}
.endif

# An undefined variable on its own generates a parse error.
# expect+1: Malformed conditional (${UNDEF})
.if ${UNDEF}
.endif

# The :U modifier turns an undefined expression into a defined expression.
# Since the expression is defined now, it doesn't generate any parse error.
.if ${UNDEF:U}
.endif

# If the value of the variable expression is a number, it is compared against
# zero.
.if ${:U0}
.  error
.endif
.if !${:U1}
.  error
.endif

# If the value of the variable expression is not a number, any non-empty
# value evaluates to true, even if there is only whitespace.
.if ${:U}
.  error
.endif
.if !${:U }
.  error
.endif
.if !${:Uanything}
.  error
.endif
